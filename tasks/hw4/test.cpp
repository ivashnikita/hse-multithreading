#include "mpsc_queue.h"

#include <gtest/gtest.h>
#include <thread>
#include <vector>

static const char* test_shm = "/hw4_test_queue";
static constexpr size_t shm_size = 65536;

class MpscQueueTest : public ::testing::Test {
protected:
    void SetUp() override {
        shm_unlink(test_shm);
    }

    void TearDown() override {
        shm_unlink(test_shm);
    }
};

TEST_F(MpscQueueTest, SingleMessage) {
    ProducerNode producer(test_shm, shm_size);
    ConsumerNode consumer(test_shm, shm_size);

    std::string msg = "hello";
    ASSERT_TRUE(producer.send(1, msg.data(), msg.size()));

    uint32_t type;
    std::vector<uint8_t> data;
    ASSERT_TRUE(consumer.receive(type, data));
    EXPECT_EQ(type, 1);
    EXPECT_EQ(std::string(data.begin(), data.end()), "hello");
}

TEST_F(MpscQueueTest, EmptyQueue) {
    ProducerNode producer(test_shm, shm_size);
    ConsumerNode consumer(test_shm, shm_size);

    uint32_t type;
    std::vector<uint8_t> data;
    EXPECT_FALSE(consumer.receive(type, data));
}

TEST_F(MpscQueueTest, MultipleMessages) {
    ProducerNode producer(test_shm, shm_size);
    ConsumerNode consumer(test_shm, shm_size);

    for (uint32_t i = 1; i <= 100; i++) {
        ASSERT_TRUE(producer.send(i, &i, sizeof(i)));
    }

    for (uint32_t i = 1; i <= 100; i++) {
        uint32_t type;
        std::vector<uint8_t> data;
        ASSERT_TRUE(consumer.receive(type, data));
        EXPECT_EQ(type, i);
        uint32_t value;
        std::memcpy(&value, data.data(), sizeof(value));
        EXPECT_EQ(value, i);
    }
}

TEST_F(MpscQueueTest, TypeFiltering) {
    ProducerNode producer(test_shm, shm_size);
    ConsumerNode consumer(test_shm, shm_size);

    uint32_t a = 10, b = 20, c = 30;
    producer.send(1, &a, sizeof(a));
    producer.send(2, &b, sizeof(b));
    producer.send(1, &c, sizeof(c));

    std::vector<uint8_t> data;
    ASSERT_TRUE(consumer.receive_by_type(2, data));
    uint32_t value;
    std::memcpy(&value, data.data(), sizeof(value));
    EXPECT_EQ(value, 20);

    // there is not other messages with this type
    ASSERT_FALSE(consumer.receive_by_type(2, data));
}

TEST_F(MpscQueueTest, MultipleProducerThreads) {
    ProducerNode producer(test_shm, shm_size);
    ConsumerNode consumer(test_shm, shm_size);

    const int threads_num = 4;
    const int msgs_per_thread = 500;
    std::atomic<int> total_sent{0};

    {
        std::vector<std::jthread> threads;
        for (int i = 0; i < threads_num; i++) {
            threads.emplace_back([&, i] {
                for (int j = 0; j < msgs_per_thread; j++) {
                    uint32_t val = i * msgs_per_thread + j;
                    while (!producer.send(1, &val, sizeof(val))) {
                        // queue is full
                    }
                    total_sent.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }
    }

    int received = 0;
    uint32_t type;
    std::vector<uint8_t> data;
    while (received < threads_num * msgs_per_thread) {
        if (consumer.receive(type, data)) {
            received++;
        }
    }

    EXPECT_EQ(received, threads_num * msgs_per_thread);
}