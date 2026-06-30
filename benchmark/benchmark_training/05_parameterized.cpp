#include <benchmark/benchmark.h>

static void BM_Sum(benchmark::State& state) {
    int n = state.range(0);
    volatile int sum = 0;
    for (auto _ : state) {
        for (int i = 0; i < n; ++i)
            sum += i;
    }
}
BENCHMARK(BM_Sum)->Range(8, 8 << 10);

static void BM_Square(benchmark::State& state) {
    int n = state.range(0);
    volatile int sum = 0;
    for (auto _ : state) {
        for (int i = 0; i < n; ++i)
            sum += i * i;
    }
}
BENCHMARK(BM_Square)->RangeMultiplier(4)->Range(16, 4096);

BENCHMARK_MAIN();
