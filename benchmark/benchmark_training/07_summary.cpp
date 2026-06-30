#include <benchmark/benchmark.h>
#include <vector>
#include <algorithm>

// 第一课：基础结构
static void BM_Empty(benchmark::State& state) {
    for (auto _ : state) {
    }
}
BENCHMARK(BM_Empty);

// 第四课：DoNotOptimize 用法
static void BM_Add(benchmark::State& state) {
    int a = 1, b = 2;
    for (auto _ : state) {
        int c = a + b;
        benchmark::DoNotOptimize(c);
    }
}
BENCHMARK(BM_Add);

// 第六课：参数化
static void BM_Sum(benchmark::State& state) {
    int n = state.range(0);
    volatile int sum = 0;
    for (auto _ : state) {
        for (int i = 0; i < n; ++i)
            sum += i;
    }
}
BENCHMARK(BM_Sum)->Range(8, 8 << 10);

// 第七课：PauseTiming
static void BM_Sort(benchmark::State& state) {
    int n = state.range(0);
    std::vector<int> v(n);
    for (auto _ : state) {
        state.PauseTiming();
        for (int i = 0; i < n; ++i) v[i] = n - i;
        state.ResumeTiming();
        std::sort(v.begin(), v.end());
        benchmark::DoNotOptimize(v.data());
    }
}
BENCHMARK(BM_Sort)->Arg(1024);

BENCHMARK_MAIN();
