#pragma once

#include <atomic>
#include <cerrno>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include <semaphore.h>
#include <sys/mman.h>

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

using deallocator_fn = void (*)(void* ptr, void* ctx);

template <typename T>
class future {
public:
    future() = default;
    explicit future(shared_state<T>* state) : state_(state) {}

    // arena based constructor
    future(shared_state<T>* state, deallocator_fn dealloc, void* dealloc_ctx)
        : state_(state), dealloc_(dealloc), dealloc_ctx_(dealloc_ctx) {}

    future(const future&) = delete;
    future& operator=(const future&) = delete;

    future(future&& other) noexcept
        : state_(other.state_), dealloc_(other.dealloc_), dealloc_ctx_(other.dealloc_ctx_) {
        other.state_ = nullptr;
    }

    future& operator=(future&& other) noexcept {
        if (this != &other) {
            release();
            state_ = other.state_;
            dealloc_ = other.dealloc_;
            dealloc_ctx_ = other.dealloc_ctx_;
            other.state_ = nullptr;
        }
        return *this;
    }

    ~future() { release(); }

    T get() {
        wait();
        auto* s = state_;
        auto dealloc = dealloc_;
        auto ctx = dealloc_ctx_;
        state_ = nullptr;
        if (s->status.load(std::memory_order_acquire) == task_status::error) {
            std::string msg(s->error_buf);
            free_state(s, dealloc, ctx);
            throw std::runtime_error(msg);
        }
        T val = std::move(s->value());
        free_state(s, dealloc, ctx);
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
    static void free_state(shared_state<T>* s, deallocator_fn dealloc, void* ctx) {
        sem_destroy(&s->sema);
        s->~shared_state();
        if (dealloc) {
            dealloc(s, ctx);
        } else {
            munmap(s, sizeof(shared_state<T>));
        }
    }

    void release() {
        if (state_) {
            free_state(state_, dealloc_, dealloc_ctx_);
            state_ = nullptr;
        }
    }

    shared_state<T>* state_ = nullptr;
    deallocator_fn dealloc_ = nullptr;
    void* dealloc_ctx_ = nullptr;
};

}