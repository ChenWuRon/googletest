#include <benchmark/benchmark.h>
#include <vector>
#include <cstring>

// BytesProcessed 吞吐量
static void BM_Memcpy_Throughput(benchmark::State& state) {
    int n = state.range(0);
    std::vector<char> src(n, 1), dst(n, 0);
    for (auto _ : state) {
        std::memcpy(dst.data(), src.data(), n);
        benchmark::DoNotOptimize(dst.data());
    }
    state.SetBytesProcessed(int64_t(state.iterations()) * n);
    state.counters["GB/s"] = benchmark::Counter(
        int64_t(state.iterations()) * n,
        benchmark::Counter::kDefaults,
        benchmark::Counter::OneK::kIs1024
    );
}
BENCHMARK(BM_Memcpy_Throughput)->Args({1048576})->Args({67108864});

// 自定义多计数器
static void BM_Request(benchmark::State& state) {
    volatile int latency = 0;
    for (auto _ : state) {
        int start = 0;
        for (int i = 0; i < 100; ++i) start += i;
        latency = start;
        benchmark::DoNotOptimize(latency);
    }
    state.counters["QPS"] = benchmark::Counter(
        state.iterations(), benchmark::Counter::kIsRate
    );
    state.counters["AvgLat"] = benchmark::Counter(
        latency, benchmark::Counter::kAvgIterations
    );
    state.counters["Threads"] = benchmark::Counter(
        state.threads(), benchmark::Counter::kAvgThreads
    );
}
BENCHMARK(BM_Request)->ThreadRange(1, 4);

BENCHMARK_MAIN();
