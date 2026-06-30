#include <benchmark/benchmark.h>
#include <mutex>
#include <atomic>

// 单线程无竞争锁
static void BM_Mutex_Single(benchmark::State& state) {
    std::mutex mtx;
    for (auto _ : state) {
        std::lock_guard<std::mutex> lock(mtx);
    }
}
BENCHMARK(BM_Mutex_Single);

// 多线程有竞争锁
static void BM_Mutex_Multi(benchmark::State& state) {
    std::mutex mtx;
    for (auto _ : state) {
        std::lock_guard<std::mutex> lock(mtx);
    }
}
BENCHMARK(BM_Mutex_Multi)->Threads(4);

// ThreadRange 多线程数
static void BM_AtomicAdd(benchmark::State& state) {
    std::atomic<long> counter{0};
    for (auto _ : state) {
        counter.fetch_add(1, std::memory_order_relaxed);
        benchmark::DoNotOptimize(counter.load());
    }
}
BENCHMARK(BM_AtomicAdd)->ThreadRange(1, 8);

// ThreadPerCpu 全核并行
static double pi_local(int n) {
    double sum = 0.0;
    for (int i = 0; i < n; ++i)
        sum += 4.0 / (1.0 + ((i + 0.5) / n) * ((i + 0.5) / n));
    return sum / n;
}

static void BM_Pi(benchmark::State& state) {
    int n = state.range(0);
    for (auto _ : state) {
        double p = pi_local(n);
        benchmark::DoNotOptimize(p);
    }
}
BENCHMARK(BM_Pi)->Arg(1000000)->ThreadPerCpu();

BENCHMARK_MAIN();
