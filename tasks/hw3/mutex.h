#pragma once

#include <atomic>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

void FutexWait(void* value, int expectedValue) {
    syscall(SYS_futex, value, FUTEX_WAIT_PRIVATE, expectedValue, nullptr, nullptr, 0);
}

void FutexWake(void* value, int count) {
    syscall(SYS_futex, value, FUTEX_WAKE_PRIVATE, count, nullptr, nullptr, 0);
}

class Mutex {
public:
    void lock() {
        if (state_.exchange(1, std::memory_order_acquire) == 0) {
            return;
        }

        while (state_.exchange(2, std::memory_order_acquire) != 0) {
            FutexWait(&state_, 2);
        }
    }

    void unlock() {
        if (state_.exchange(0, std::memory_order_release) == 2) {
            FutexWake(&state_, 1);
        }
    }

private:
    std::atomic<int> state_{0};
};