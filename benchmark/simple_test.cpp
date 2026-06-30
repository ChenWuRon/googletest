#include <benchmark/benchmark.h>
#include <vector>
#include <list>
#include <unordered_map>
#include <map>
#include <memory>
#include <algorithm>
#include <random>

// 1. vector vs list
static void BM_VectorPush(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<int> v;
        for (int i = 0; i < state.range(0); ++i) v.push_back(i);
        benchmark::DoNotOptimize(v.data());
    }
}
BENCHMARK(BM_VectorPush)->Arg(100000);

static void BM_ListPush(benchmark::State& state) {
    for (auto _ : state) {
        std::list<int> l;
        for (int i = 0; i < state.range(0); ++i) l.push_back(i);
        benchmark::DoNotOptimize(&l);
    }
}
BENCHMARK(BM_ListPush)->Arg(100000);

// 2. map vs unordered_map
static void BM_MapInsert(benchmark::State& state) {
    std::map<int, int> m;
    int i = 0;
    for (auto _ : state) {
        m[i++ % 100000] = i;
    }
}
BENCHMARK(BM_MapInsert);

static void BM_UMapInsert(benchmark::State& state) {
    std::unordered_map<int, int> m;
    int i = 0;
    for (auto _ : state) {
        m[i++ % 100000] = i;
    }
}
BENCHMARK(BM_UMapInsert);

// 3. shared_ptr vs unique_ptr
static void BM_SharedPtr(benchmark::State& state) {
    for (auto _ : state) {
        auto p = std::make_shared<int>(42);
        benchmark::DoNotOptimize(p);
    }
}
BENCHMARK(BM_SharedPtr);

static void BM_UniquePtr(benchmark::State& state) {
    for (auto _ : state) {
        auto p = std::make_unique<int>(42);
        benchmark::DoNotOptimize(p);
    }
}
BENCHMARK(BM_UniquePtr);

// 4. sort vs stable_sort
static void BM_Sort(benchmark::State& state) {
    std::vector<int> v(state.range(0));
    for (auto _ : state) {
        state.PauseTiming();
        std::mt19937 rng(42);
        for (auto& x : v) x = rng();
        state.ResumeTiming();
        std::sort(v.begin(), v.end());
        benchmark::DoNotOptimize(v.data());
    }
}
BENCHMARK(BM_Sort)->Arg(100000);

static void BM_StableSort(benchmark::State& state) {
    std::vector<int> v(state.range(0));
    for (auto _ : state) {
        state.PauseTiming();
        std::mt19937 rng(42);
        for (auto& x : v) x = rng();
        state.ResumeTiming();
        std::stable_sort(v.begin(), v.end());
        benchmark::DoNotOptimize(v.data());
    }
}
BENCHMARK(BM_StableSort)->Arg(100000);

// 5. reserve vs no reserve
static void BM_NoReserve(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<int> v;
        for (int i = 0; i < state.range(0); ++i) v.push_back(i);
        benchmark::DoNotOptimize(v.data());
    }
}
BENCHMARK(BM_NoReserve)->Arg(100000);

static void BM_Reserve(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<int> v;
        v.reserve(state.range(0));
        for (int i = 0; i < state.range(0); ++i) v.push_back(i);
        benchmark::DoNotOptimize(v.data());
    }
}
BENCHMARK(BM_Reserve)->Arg(100000);

BENCHMARK_MAIN();