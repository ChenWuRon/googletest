#include <benchmark/benchmark.h>
#include <vector>
#include <algorithm>

// std::sort — 期望 O(NlogN)
static void BM_StdSort(benchmark::State& state) {
    std::vector<int> v(state.range(0));
    for (auto _ : state) {
        state.PauseTiming();
        for (int i = 0; i < v.size(); ++i) v[i] = v.size() - i;
        state.ResumeTiming();
        std::sort(v.begin(), v.end());
        benchmark::DoNotOptimize(v.data());
    }
    state.SetComplexityN(state.range(0));
}
BENCHMARK(BM_StdSort)
    ->RangeMultiplier(2)->Range(1 << 10, 1 << 18)
    ->Complexity();

// 冒泡排序 — 期望 O(N²)
static void BM_BubbleSort(benchmark::State& state) {
    std::vector<int> v(state.range(0));
    for (auto _ : state) {
        state.PauseTiming();
        for (int i = 0; i < v.size(); ++i) v[i] = v.size() - i;
        state.ResumeTiming();
        for (size_t i = 0; i < v.size(); ++i) {
            for (size_t j = 0; j < v.size() - i - 1; ++j) {
                if (v[j] > v[j + 1])
                    std::swap(v[j], v[j + 1]);
            }
        }
        benchmark::DoNotOptimize(v.data());
    }
    state.SetComplexityN(state.range(0));
}
BENCHMARK(BM_BubbleSort)
    ->RangeMultiplier(2)->Range(1 << 4, 1 << 10)
    ->Complexity();

BENCHMARK_MAIN();