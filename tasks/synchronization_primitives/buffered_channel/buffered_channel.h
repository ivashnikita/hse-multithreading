#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>

template <class T>
class BufferedChannel {
public:
    explicit BufferedChannel(int size) : capacity_(size) {
    }

    void Send(const T& value) {
        std::unique_lock lock(m_);
        send_cv_.wait(lock, [this] { return closed_ || static_cast<int>(queue_.size()) < capacity_; });
        
        if (closed_) {
            throw std::runtime_error("channel is closed!");
        }
        
        queue_.push(value);
        recv_cv_.notify_one();
    }

    std::optional<T> Recv() {
        std::unique_lock lock(m_);
        recv_cv_.wait(lock, [this] { return closed_ || !queue_.empty(); });
        
        if (queue_.empty()) {
            return std::nullopt;
        }
        
        T value = std::move(queue_.front());
        queue_.pop();
        send_cv_.notify_one();
        
        return value;
    }

    void Close() {
        std::lock_guard lock(m_);
        closed_ = true;
        send_cv_.notify_all();
        recv_cv_.notify_all();
    }

private:
    int capacity_;
    bool closed_{false};

    std::queue<T> queue_;
    std::mutex m_;
    std::condition_variable send_cv_;
    std::condition_variable recv_cv_;
};