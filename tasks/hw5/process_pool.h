#pragma once

#include <atomic>
#include <cerrno>
#include <cstring>
#include <functional>
#include <mutex>
#include <new>
#include <semaphore>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include <semaphore.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

namespace pp {

enum class task_status : uint8_t {
    pending = 0,
    ready = 1,
    error = 2,
};

template <typename T>
struct shared_state {
    std::atomic<task_status> status{task_status::pending};
    // POSIX semaphore to share it between processes
    sem_t sema;
    alignas(T) char value_buf[sizeof(T)];
    char error_buf[256];

    T& value() { return *std::launder(reinterpret_cast<T*>(value_buf)); }

    void set_value(T val) {
        new (value_buf) T(std::move(val));
        status.store(task_status::ready, std::memory_order_release);
        sem_post(&sema);
    }

    void set_error(const char* msg) {
        std::strncpy(error_buf, msg, sizeof(error_buf) - 1);
        error_buf[sizeof(error_buf) - 1] = '\0';
        status.store(task_status::error, std::memory_order_release);
        sem_post(&sema);
    }

    ~shared_state() {
        if (status.load(std::memory_order_acquire) == task_status::ready) {
            value().~T();
        }
    }
};

// if task doesn't return anything
template <>
struct shared_state<void> {
    std::atomic<task_status> status{task_status::pending};
    sem_t sema;
    char error_buf[256];

    void set_value() {
        status.store(task_status::ready, std::memory_order_release);
        sem_post(&sema);
    }

    void set_error(const char* msg) {
        std::strncpy(error_buf, msg, sizeof(error_buf) - 1);
        error_buf[sizeof(error_buf) - 1] = '\0';
        status.store(task_status::error, std::memory_order_release);
        sem_post(&sema);
    }
};

template <typename T>
shared_state<T>* allocate_shared_state() {
    void* mem = mmap(nullptr, sizeof(shared_state<T>),
                     PROT_READ | PROT_WRITE,
                     MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        throw std::runtime_error(std::string("mmap failed: ") + std::strerror(errno));
    }

    auto* state = new (mem) shared_state<T>();
    // sem works between processes
    if (sem_init(&state->sema, 1, 0) == -1) {
        munmap(mem, sizeof(shared_state<T>));
        throw std::runtime_error(std::string("sem_init failed: ") + std::strerror(errno));
    }

    return state;
}

template <typename T>
void deallocate_shared_state(shared_state<T>* state) {
    if (!state) {
    	return;
    }
    sem_destroy(&state->sema);
    state->~shared_state();
    munmap(state, sizeof(shared_state<T>));
}

template <typename T>
class future {
public:
    future() = default;
    explicit future(shared_state<T>* state) : state_(state) {}

    future(const future&) = delete;
    future& operator=(const future&) = delete;

    future(future&& other) noexcept : state_(other.state_) {
        other.state_ = nullptr;
    }

    future& operator=(future&& other) noexcept {
        if (this != &other) {
            release();
            state_ = other.state_;
            other.state_ = nullptr;
        }
        return *this;
    }

    ~future() { release(); }

    T get() {
        wait();
        auto* s = state_;
        state_ = nullptr;
        if (s->status.load(std::memory_order_acquire) == task_status::error) {
            std::string msg(s->error_buf);
            deallocate_shared_state(s);
            throw std::runtime_error(msg);
        }
        T val = std::move(s->value());
        deallocate_shared_state(s);
        return val;
    }

    void wait() const {
        if (!state_) {
        	throw std::runtime_error("future has no shared state");
        }
        while (sem_wait(&state_->sema) == -1) {
            if (errno != EINTR) {
                throw std::runtime_error(std::string("sem_wait failed: ") + std::strerror(errno));
            }
        }

        // to continue work
        sem_post(&state_->sema);
    }

    // it's convenient use in tests
    bool is_ready() const {
        if (!state_) {
         return false;
        }
        return state_->status.load(std::memory_order_acquire) != task_status::pending;
    }

    bool valid() const { return state_ != nullptr; }

private:
    void release() {
        if (state_) {
            deallocate_shared_state(state_);
            state_ = nullptr;
        }
    }

    shared_state<T>* state_ = nullptr;
};

class process_pool {
public:
    explicit process_pool(size_t max_workers)
        : worker_slots_(static_cast<std::ptrdiff_t>(max_workers)) {}

    ~process_pool() {
        shutdown();
    }

    process_pool(const process_pool&) = delete;
    process_pool& operator=(const process_pool&) = delete;

    template <typename F, typename... Args>
    requires std::invocable<F, Args...>
    auto submit(F&& func, Args&&... args) {
        using result_t = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;

        auto* state = allocate_shared_state<result_t>();

        auto bound = std::bind(std::forward<F>(func), std::forward<Args>(args)...);

        acquire_slot();

        pid_t pid = fork();
        if (pid == -1) {
            worker_slots_.release();
            deallocate_shared_state(state);
            throw std::runtime_error(std::string("fork failed: ") + std::strerror(errno));
        }

        // child
        if (pid == 0) {
            execute_task(state, std::move(bound));
            _exit(0);
        }

        {
            std::lock_guard lock(mutex_);
            children_.push_back(pid);
        }

        return future<result_t>(state);
    }

private:
    template <typename T, typename Callable>
    static void execute_task(shared_state<T>* state, Callable callable) {
        try {
            if constexpr (std::is_void_v<T>) {
                callable();
                state->set_value();
            } else {
                state->set_value(callable());
            }
        } catch (const std::exception& e) {
            state->set_error(e.what());
        } catch (...) {
            state->set_error("unknown exception");
        }
    }

    void acquire_slot() {
        if (worker_slots_.try_acquire()) return;

        // all slots are busy - block until any child exits
        int status;
        pid_t pid = waitpid(-1, &status, 0);
        if (pid > 0) {
            std::lock_guard lock(mutex_);
            std::erase(children_, pid);
            worker_slots_.release();
        }

        worker_slots_.acquire();
    }

    void shutdown() {
        int status;
        while (waitpid(-1, &status, 0) > 0) {}
    }

    std::counting_semaphore<> worker_slots_;
    std::mutex mutex_;
    std::vector<pid_t> children_;
};

}