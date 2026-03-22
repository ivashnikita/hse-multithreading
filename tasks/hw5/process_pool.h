#pragma once

#include "future.h"

#include <cstdint>
#include <cstring>
#include <functional>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include <semaphore.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

namespace pp {

struct shared_arena {
    static constexpr size_t block_size = 512;

    std::atomic_flag lock{};
    size_t num_blocks;

    struct node { node* next; };
    node* free_list;

    void acquire() { while (lock.test_and_set(std::memory_order_acquire)) {} }
    void release() { lock.clear(std::memory_order_release); }

    void* allocate() {
        acquire();
        if (!free_list) {
          release();
          return nullptr;
        }

        node* n = free_list;
        free_list = n->next;
        release();

        return n;
    }

    void deallocate(void* ptr) {
        if (!ptr) {
            return;
        }
        auto* n = static_cast<node*>(ptr);

        acquire();
        n->next = free_list;
        free_list = n;
        release();
    }
};

// ring buffer queue
struct task_queue {
    static constexpr size_t max_size = 1024;

    std::atomic_flag lock{};
    sem_t sem;
    size_t head = 0;
    size_t tail = 0;
    size_t count = 0;
    void* tasks[max_size];

    void acquire() { while (lock.test_and_set(std::memory_order_acquire)) {} }
    void release() { lock.clear(std::memory_order_release); }

    bool push(void* task) {
        acquire();
        if (count >= max_size) {
            release();
            return false;
        }

        tasks[tail] = task;
        tail = (tail + 1) % max_size;
        count++;
        release();

        sem_post(&sem);
        return true;
    }

    void* pop() {
        while (sem_wait(&sem) == -1) {
            if (errno != EINTR) {
                return nullptr;
            }
        }

        acquire();
        void* task = tasks[head];
        head = (head + 1) % max_size;
        count--;
        release();

        return task;
    }
};

// Type erased task header
struct task_base {
    void (*execute)(task_base*);
    shared_arena* arena;
};

template <typename F, typename T>
struct task_impl : task_base {
    shared_state<T>* state;
    F callable;

    static void run(task_base* self) {
        auto* t = static_cast<task_impl*>(self);
        try {
            if constexpr (std::is_void_v<T>) {
                t->callable();
                t->state->set_value();
            } else {
                t->state->set_value(t->callable());
            }
        } catch (const std::exception& e) {
            t->state->set_error(e.what());
        } catch (...) {
            t->state->set_error("unknown exception");
        }
    }
};

class process_pool {
    static constexpr size_t arena_blocks = 1024;

public:
    explicit process_pool(size_t num_workers) {
        // allocate shared region before fork. There are arena and queue
        size_t arena_total = sizeof(shared_arena) + shared_arena::block_size * arena_blocks;
        size_t region_size = arena_total + sizeof(task_queue);

        region_ = static_cast<char*>(mmap(nullptr, region_size,
                     PROT_READ | PROT_WRITE,
                     MAP_SHARED | MAP_ANONYMOUS, -1, 0));
        if (region_ == MAP_FAILED) {
            throw std::runtime_error(std::string("mmap failed: ") + std::strerror(errno));
        }
        region_size_ = region_size;

        // init arena
        arena_ = new (region_) shared_arena();
        arena_->num_blocks = arena_blocks;
        char* blocks = region_ + sizeof(shared_arena);
        arena_->free_list = nullptr;
        for (size_t i = 0; i < arena_blocks; i++) {
            auto* n = reinterpret_cast<shared_arena::node*>(blocks + i * shared_arena::block_size);
            n->next = arena_->free_list;
            arena_->free_list = n;
        }

        // init queue
        queue_ = new (region_ + arena_total) task_queue();
        if (sem_init(&queue_->sem, 1, 0) == -1) {
            munmap(region_, region_size_);
            throw std::runtime_error(std::string("sem_init failed: ") + std::strerror(errno));
        }

        workers_.reserve(num_workers);
        for (size_t i = 0; i < num_workers; i++) {
            pid_t pid = fork();
            if (pid == -1) {
                shutdown();
                throw std::runtime_error(std::string("fork failed: ") + std::strerror(errno));
            }
            if (pid == 0) {
                worker_loop(queue_);
                _exit(0);
            }

            workers_.push_back(pid);
        }
    }

    ~process_pool() {
        shutdown();
    }

    process_pool(const process_pool&) = delete;
    process_pool& operator=(const process_pool&) = delete;

    template <typename F, typename... Args>
    requires std::invocable<F, Args...>
    auto submit(F&& func, Args&&... args) {
        using result_t = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;
        using bound_t = decltype(std::bind(std::forward<F>(func), std::forward<Args>(args)...));
        using impl_t = task_impl<bound_t, result_t>;

        static_assert(sizeof(impl_t) <= shared_arena::block_size, "task too large for arena block");
        static_assert(sizeof(shared_state<result_t>) <= shared_arena::block_size, "shared_state too large for arena block");

        // separate allocation because lifetime of task and state is different
        void* state_mem = arena_->allocate();
        if (!state_mem) {
            throw std::runtime_error("arena: out of memory for shared_state");
        }
        auto* state = new (state_mem) shared_state<result_t>();
        if (sem_init(&state->sema, 1, 0) == -1) {
            arena_->deallocate(state_mem);
            throw std::runtime_error(std::string("sem_init failed: ") + std::strerror(errno));
        }

        void* task_mem = arena_->allocate();
        if (!task_mem) {
            sem_destroy(&state->sema);
            state->~shared_state<result_t>();
            arena_->deallocate(state_mem);
            throw std::runtime_error("arena: out of memory for task");
        }

        auto* task = static_cast<impl_t*>(task_mem);
        task->execute = &impl_t::run;
        task->arena = arena_;
        task->state = state;
        new (&task->callable) bound_t(std::bind(std::forward<F>(func), std::forward<Args>(args)...));

        if (!queue_->push(task)) {
            arena_->deallocate(task_mem);
            sem_destroy(&state->sema);
            state->~shared_state<result_t>();
            arena_->deallocate(state_mem);
            throw std::runtime_error("task queue is full");
        }

        return future<result_t>(state, arena_deallocator, arena_);
    }

private:
    static void arena_deallocator(void* ptr, void* ctx) {
        static_cast<shared_arena*>(ctx)->deallocate(ptr);
    }

    static void worker_loop(task_queue* queue) {
        while (true) {
            void* raw = queue->pop();
            if (!raw) {
                break;
            }

            auto* task = static_cast<task_base*>(raw);
            task->execute(task);
            task->arena->deallocate(task);
        }
    }

    void shutdown() {
        // stop each worker with nullptr
        for (size_t i = 0; i < workers_.size(); i++) {
            queue_->push(nullptr);
        }

        for (pid_t pid : workers_) {
            if (pid > 0) {
                waitpid(pid, nullptr, 0);
            }
        }
        workers_.clear();

        if (region_) {
            sem_destroy(&queue_->sem);
            munmap(region_, region_size_);
            region_ = nullptr;
        }
    }

    char* region_ = nullptr;
    size_t region_size_ = 0;
    shared_arena* arena_ = nullptr;
    task_queue* queue_ = nullptr;
    std::vector<pid_t> workers_;
};

}