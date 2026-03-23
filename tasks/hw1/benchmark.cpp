#include "apply_function.h"

#include <benchmark/benchmark.h>

#include <cmath>
#include <numeric>

// Лёгкая функция, поэтому один поток быстрее из-за отсутствия накладных расходов на создание потоков
static void BM_LightTransform_1Thread(benchmark::State& state) {
    std::vector<int> data(10000);
    for (auto _ : state) {
        std::iota(data.begin(), data.end(), 0);
        ApplyFunction<int>(data, [](int& x) { x += 1; }, 1);
        benchmark::DoNotOptimize(data.data());
    }
}

static void BM_LightTransform_4Threads(benchmark::State& state) {
    std::vector<int> data(10000);
    for (auto _ : state) {
        std::iota(data.begin(), data.end(), 0);
        ApplyFunction<int>(data, [](int& x) { x += 1; }, 4);
        benchmark::DoNotOptimize(data.data());
    }
}

// Тяжёлая функция, поэтому в 4 потока быстрее
static void HeavyTransform(double& x) {
    for (int i = 1; i < 500; ++i) {
        x = std::atan(x) + 1.5;
    }
}

static void BM_HeavyTransform_1Thread(benchmark::State& state) {
    std::vector<double> data(10000, 1.0);
    for (auto _ : state) {
        std::fill(data.begin(), data.end(), 1.0);
        ApplyFunction<double>(data, HeavyTransform, 1);
        benchmark::DoNotOptimize(data.data());
    }
}

static void BM_HeavyTransform_4Threads(benchmark::State& state) {
    std::vector<double> data(10000, 1.0);
    for (auto _ : state) {
        std::fill(data.begin(), data.end(), 1.0);
        ApplyFunction<double>(data, HeavyTransform, 4);
        benchmark::DoNotOptimize(data.data());
    }
}

BENCHMARK(BM_LightTransform_1Thread);
BENCHMARK(BM_LightTransform_4Threads);
BENCHMARK(BM_HeavyTransform_1Thread);
BENCHMARK(BM_HeavyTransform_4Threads);

// Маленький вектор, поэтому один поток быстрее по аналогичным причинам (расходы на создание потоков больше)
static void MediumTransform(double& x) {
    for (int i = 0; i < 50; ++i) {
        x = std::sin(x) + 1.0;
    }
}

static void BM_SmallVector_1Thread(benchmark::State& state) {
    std::vector<double> data(50, 1.0);
    for (auto _ : state) {
        std::fill(data.begin(), data.end(), 1.0);
        ApplyFunction<double>(data, MediumTransform, 1);
        benchmark::DoNotOptimize(data.data());
    }
}

static void BM_SmallVector_4Threads(benchmark::State& state) {
    std::vector<double> data(50, 1.0);
    for (auto _ : state) {
        std::fill(data.begin(), data.end(), 1.0);
        ApplyFunction<double>(data, MediumTransform, 4);
        benchmark::DoNotOptimize(data.data());
    }
}

// Большой вектор, поэтому в 4 потока быстрее
static void BM_LargeVector_1Thread(benchmark::State& state) {
    std::vector<double> data(100000, 1.0);
    for (auto _ : state) {
        std::fill(data.begin(), data.end(), 1.0);
        ApplyFunction<double>(data, MediumTransform, 1);
        benchmark::DoNotOptimize(data.data());
    }
}

static void BM_LargeVector_4Threads(benchmark::State& state) {
    std::vector<double> data(100000, 1.0);
    for (auto _ : state) {
        std::fill(data.begin(), data.end(), 1.0);
        ApplyFunction<double>(data, MediumTransform, 4);
        benchmark::DoNotOptimize(data.data());
    }
}

BENCHMARK(BM_SmallVector_1Thread);
BENCHMARK(BM_SmallVector_4Threads);
BENCHMARK(BM_LargeVector_1Thread);
BENCHMARK(BM_LargeVector_4Threads);
