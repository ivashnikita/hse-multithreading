#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static constexpr uint32_t protocol_version_current = 1;
static constexpr uint32_t msg_type_skip = 0;

inline uint64_t align_up(uint64_t value, uint64_t alignment) {
    uint64_t remainder = value % alignment;
    if (remainder == 0) {
        return value;
    }
    return value + (alignment - remainder);
}

struct MessageHeader {
    uint32_t msg_type;
    uint32_t payload_length;
    uint32_t total_size;
    std::atomic<uint8_t> committed;
    uint8_t _pad[3];
};

static_assert(sizeof(MessageHeader) == 16);

struct ShmHeader {
    uint32_t protocol_version;
    uint32_t buffer_capacity;
    alignas(64) std::atomic<uint64_t> write_pos;
    alignas(64) std::atomic<uint64_t> read_pos;
};

// The start of aligned ring buffer after header
inline char* ring_buffer(ShmHeader* hdr) {
    return reinterpret_cast<char*>(hdr) + align_up(sizeof(ShmHeader), 64);
}

inline uint64_t total_message_size(uint32_t payload_length) {
    // we use aligning because we want to prevent situation when there is
    // not enough space for msg header in the end
    return align_up(sizeof(MessageHeader) + payload_length, sizeof(MessageHeader));
}

class ProducerNode {
public:
    ProducerNode(const char* shm_path, size_t size, bool create = true) : shm_path_(shm_path), shm_size_(size) {
        int flags = create ? (O_CREAT | O_RDWR) : O_RDWR;
        int fd = shm_open(shm_path, flags, 0666);
        if (fd < 0) {
            throw std::runtime_error("shm_open failed");
        }

        if (create && ftruncate(fd, shm_size_) < 0) {
            close(fd);
            throw std::runtime_error("ftruncate failed");
        }

        void* ptr = mmap(nullptr, shm_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        close(fd);
        if (ptr == MAP_FAILED) {
            throw std::runtime_error("mmap failed");
        }

        hdr_ = static_cast<ShmHeader*>(ptr);

        if (create) {
            hdr_->protocol_version = protocol_version_current;
            uint64_t raw_capacity = shm_size_ - align_up(sizeof(ShmHeader), 64);
            hdr_->buffer_capacity = raw_capacity - raw_capacity % sizeof(MessageHeader);
            hdr_->write_pos.store(0, std::memory_order_relaxed);
            hdr_->read_pos.store(0, std::memory_order_relaxed);
        } else if (hdr_->protocol_version != protocol_version_current) {
            munmap(hdr_, shm_size_);
            hdr_ = nullptr;
            throw std::runtime_error("protocol version mismatch");
        }
    }

    ~ProducerNode() {
        if (hdr_) {
            munmap(hdr_, shm_size_);
        }
    }

    ProducerNode(const ProducerNode&) = delete;
    ProducerNode& operator=(const ProducerNode&) = delete;

    bool send(uint32_t msg_type, const void* data, uint32_t len) {
        uint64_t msg_size = total_message_size(len);
        uint32_t capacity = hdr_->buffer_capacity;

        if (msg_size > capacity) {
            return false;
        }

        while (true) {
            uint64_t wp = hdr_->write_pos.load(std::memory_order_relaxed);
            uint64_t rp = hdr_->read_pos.load(std::memory_order_acquire);

            uint64_t offset = wp % capacity;
            uint64_t remaining = capacity - offset;

            // use skip-marker to try to put message by using advantage of ring buffer
            if (remaining < msg_size) {
                uint64_t skip_total = remaining;
                // there isn't free space even with skip marker
                if (rp + capacity - wp < skip_total + msg_size) {
                    return false;
                }
                if (!hdr_->write_pos.compare_exchange_weak(wp, wp + skip_total, std::memory_order_relaxed)) {
                    continue;
                }

                // Write skip-marker
                char* slot = ring_buffer(hdr_) + offset;
                auto* mhdr = reinterpret_cast<MessageHeader*>(slot);
                mhdr->msg_type = msg_type_skip;
                mhdr->payload_length = skip_total - sizeof(MessageHeader);
                mhdr->total_size = skip_total;
                mhdr->committed.store(1, std::memory_order_release);

                // retry, now write from the start of buffer
                continue;
            }

            if (rp + capacity - wp < msg_size) {
                return false;
            }

            if (!hdr_->write_pos.compare_exchange_weak(wp, wp + msg_size, std::memory_order_relaxed)) {
                continue;
            }

            char* slot = ring_buffer(hdr_) + offset;
            auto* mhdr = reinterpret_cast<MessageHeader*>(slot);
            mhdr->msg_type = msg_type;
            mhdr->payload_length = len;
            mhdr->total_size = msg_size;
            if (len > 0) {
                std::memcpy(slot + sizeof(MessageHeader), data, len);
            }
            mhdr->committed.store(1, std::memory_order_release);

            return true;
        }
    }

    void unlink() {
        shm_unlink(shm_path_.c_str());
    }

private:
    std::string shm_path_;
    size_t shm_size_;
    ShmHeader* hdr_ = nullptr;
};

class ConsumerNode {
public:
    ConsumerNode(const char* shm_path, size_t size) : shm_size_(size) {
        int fd = shm_open(shm_path, O_RDWR, 0666);
        if (fd < 0) {
            throw std::runtime_error("shm_open failed");
        }

        void* ptr = mmap(nullptr, shm_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        close(fd);
        if (ptr == MAP_FAILED) {
            throw std::runtime_error("mmap failed");
        }
        hdr_ = static_cast<ShmHeader*>(ptr);

        if (hdr_->protocol_version != protocol_version_current) {
            munmap(hdr_, shm_size_);
            hdr_ = nullptr;
            throw std::runtime_error("protocol version mismatch");
        }
    }

    ~ConsumerNode() {
        if (hdr_) {
            munmap(hdr_, shm_size_);
        }
    }

    ConsumerNode(const ConsumerNode&) = delete;
    ConsumerNode& operator=(const ConsumerNode&) = delete;


    bool receive(uint32_t& out_type, std::vector<uint8_t>& out_data) {
        return receive_impl(std::nullopt, out_type, out_data);
    }

    bool receive_by_type(uint32_t filter_type, std::vector<uint8_t>& out_data) {
        uint32_t type;
        return receive_impl(filter_type, type, out_data);
    }

private:
    bool receive_impl(std::optional<uint32_t> filter_type, uint32_t& out_type, std::vector<uint8_t>& out_data) {
        uint32_t capacity = hdr_->buffer_capacity;

        while (true) {
            uint64_t rp = hdr_->read_pos.load(std::memory_order_relaxed);
            uint64_t wp = hdr_->write_pos.load(std::memory_order_acquire);

            // there is no messages
            if (rp == wp) {
                return false;
            }

            uint64_t offset = rp % capacity;
            char* slot = ring_buffer(hdr_) + offset;
            auto* mhdr = reinterpret_cast<MessageHeader*>(slot);

            // Waiting when producer stop writing
            while (mhdr->committed.load(std::memory_order_acquire) == 0) {
            }

            uint64_t msg_size = mhdr->total_size;

            // Skip skip-marker
            if (mhdr->msg_type == msg_type_skip) {
                // for reusing for the next cycle of ring buffer
                mhdr->committed.store(0, std::memory_order_relaxed);
                hdr_->read_pos.store(rp + msg_size, std::memory_order_release);
                continue;
            }

            // Filters
            if (filter_type.has_value() && mhdr->msg_type != *filter_type) {
                mhdr->committed.store(0, std::memory_order_relaxed);
                hdr_->read_pos.store(rp + msg_size, std::memory_order_release);
                continue;
            }

            out_type = mhdr->msg_type;
            out_data.assign(slot + sizeof(MessageHeader), slot + sizeof(MessageHeader) + mhdr->payload_length);
            mhdr->committed.store(0, std::memory_order_relaxed);
            hdr_->read_pos.store(rp + msg_size, std::memory_order_release);

            return true;
        }
    }

    size_t shm_size_;
    ShmHeader* hdr_ = nullptr;
};
