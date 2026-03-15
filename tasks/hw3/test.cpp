#include "mutex.h"

#include <gtest/gtest.h>
#include <thread>
#include <vector>

TEST(MutexTest, BasicLockUnlock) {
    Mutex m;
    m.lock();
    m.unlock();
}

TEST(MutexTest, BasicUsage) {
    Mutex m;
    int counter = 0;
    const int threads_num = 4;
    const int it_num = 100'000;

    {
        std::vector<std::jthread> threads;
        for (int i = 0; i < threads_num; i++) {
            threads.emplace_back([&]{
                for (int j = 0; j < it_num; j++) {
                    m.lock();
                    counter++;
                    m.unlock();
                }
            });
        }
    }

    EXPECT_EQ(counter, threads_num * it_num);
}