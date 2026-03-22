#include "process_pool.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <numeric>
#include <set>
#include <thread>
#include <vector>

#include <unistd.h>

TEST(Future, BasicGetValue) {
    auto* state = pp::allocate_shared_state<int>();
    pp::future<int> fut(state);

    state->set_value(42);

    EXPECT_TRUE(fut.is_ready());
    EXPECT_EQ(fut.get(), 42);
}

TEST(Future, ErrorPropagation) {
    auto* state = pp::allocate_shared_state<int>();
    pp::future<int> fut(state);

    state->set_error("test error");

    EXPECT_TRUE(fut.is_ready());
    EXPECT_THROW(fut.get(), std::runtime_error);
}

TEST(Future, MoveOnly) {
    auto* state = pp::allocate_shared_state<int>();
    pp::future<int> fut1(state);
    state->set_value(10);

    pp::future<int> fut2 = std::move(fut1);
    EXPECT_FALSE(fut1.valid());
    EXPECT_TRUE(fut2.valid());
    EXPECT_EQ(fut2.get(), 10);
}

TEST(ProcessPool, SingleTask) {
    pp::process_pool pool(2);
    auto fut = pool.submit([] { return 2 + 2; });
    EXPECT_EQ(fut.get(), 4);
}

TEST(ProcessPool, MultipleTasks) {
    pp::process_pool pool(4);

    std::vector<pp::future<int>> futures;
    for (int i = 0; i < 10; ++i) {
        futures.push_back(pool.submit([i] { return i * i; }));
    }

    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(futures[i].get(), i * i);
    }
}

TEST(ProcessPool, VoidTask) {
    pp::process_pool pool(2);
    auto fut = pool.submit([] {});
    EXPECT_NO_THROW(fut.wait());
}

TEST(ProcessPool, TaskWithArguments) {
    pp::process_pool pool(2);
    auto fut = pool.submit([](int a, int b) { return a + b; }, 17, 25);
    EXPECT_EQ(fut.get(), 42);
}

TEST(ProcessPool, ExceptionInTask) {
    pp::process_pool pool(2);
    auto fut = pool.submit([] -> int {
        throw std::runtime_error("task failed");
    });
    EXPECT_THROW(fut.get(), std::runtime_error);
}

TEST(ProcessPool, WorkersAreReused) {
    constexpr size_t num_workers = 2;
    constexpr int num_tasks = 20;

    pp::process_pool pool(num_workers);

    std::vector<pp::future<pid_t>> futures;
    for (int i = 0; i < num_tasks; ++i) {
        futures.push_back(pool.submit([] { return getpid(); }));
    }

    std::set<pid_t> unique_pids;
    for (auto& f : futures) {
        unique_pids.insert(f.get());
    }

    // check that it's really pre-forked process pool
    EXPECT_LE(unique_pids.size(), num_workers);
}

TEST(ProcessPool, LargeNumberOfTasks) {
    pp::process_pool pool(8);

    std::vector<pp::future<int>> futures;
    constexpr int n = 50;
    for (int i = 0; i < n; ++i) {
        futures.push_back(pool.submit([i] { return i * 2; }));
    }

    for (int i = 0; i < n; ++i) {
        EXPECT_EQ(futures[i].get(), i * 2);
    }
}