#pragma once

#include <functional>
#include <thread>
#include <vector>

template <typename T>
void ApplyFunction(std::vector<T>& data, const std::function<void(T&)>& transform, const int threadCount = 1) {
    const int size = static_cast<int>(data.size());
    if (size == 0) {
        return;
    }

    const int effectiveThreads = std::min(threadCount, size);

    if (effectiveThreads <= 1) {
        for (auto& elem : data) {
            transform(elem);
        }

        return;
    }

    const int chunkSize = size / effectiveThreads;
    const int remainder = size % effectiveThreads;

    std::vector<std::thread> threads;
    threads.reserve(effectiveThreads);

    int start = 0;
    for (int i = 0; i < effectiveThreads; ++i) {
        int end = start + chunkSize + (i < remainder ? 1 : 0);

        threads.emplace_back([&data, &transform, start, end]() {
            for (int j = start; j < end; ++j) {
                transform(data[j]);
            }
        });

        start = end;
    }

    for (auto& t : threads) {
        t.join();
    }
}