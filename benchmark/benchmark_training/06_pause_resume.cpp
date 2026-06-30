#include <benchmark/benchmark.h>
#include <vector>
#include <algorithm>

static void BM_Sort_Wrong(benchmark::State& state) {
    int n = state.range(0);
    for (auto _ : state) {
        std::vector<int> v(n);
        for (int i = 0; i < n; ++i) v[i] = n - i;
        std::sort(v.begin(), v.end());
        benchmark::DoNotOptimize(v.data());
    }
}
BENCHMARK(BM_Sort_Wrong)->Arg(1024);

static void BM_Sort_Correct(benchmark::State& state) {
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
BENCHMARK(BM_Sort_Correct)->Arg(1024);

static void BM_Sort_Middle(benchmark::State& state) {
    int n = state.range(0);
    std::vector<int> v(n);
    for (auto _ : state) {
        for (int i = 0; i < n; ++i) v[i] = n - i;
        std::sort(v.begin(), v.end());
        benchmark::DoNotOptimize(v.data());
    }
}
BENCHMARK(BM_Sort_Middle)->Arg(1024);

BENCHMARK_MAIN();
