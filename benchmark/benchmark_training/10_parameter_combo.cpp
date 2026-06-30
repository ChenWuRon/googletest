#include <benchmark/benchmark.h>
#include <vector>
#include <cstring>
#include <algorithm>

// Args 手动指定
static void BM_Sort(benchmark::State& state) {
    std::vector<int> v(state.range(0));
    for (auto _ : state) {
        state.PauseTiming();
        for (size_t i = 0; i < v.size(); ++i) v[i] = v.size() - i;
        state.ResumeTiming();
        std::sort(v.begin(), v.end());
        benchmark::DoNotOptimize(v.data());
    }
}
BENCHMARK(BM_Sort)->Args({100})->Args({1000})->Args({10000});

// DenseRange 连续整数
static void BM_Loop(benchmark::State& state) {
    int n = state.range(0);
    volatile int sum = 0;
    for (auto _ : state) {
        for (int i = 0; i < n; ++i) sum += i;
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_Loop)->DenseRange(10, 50, 10);

// Ranges 多维组合
static void BM_MatMul(benchmark::State& state) {
    int M = state.range(0), N = state.range(1), K = state.range(2);
    std::vector<double> A(M * K, 1.0), B(K * N, 2.0), C(M * N, 0.0);
    for (auto _ : state) {
        for (int i = 0; i < M; ++i)
            for (int j = 0; j < N; ++j)
                for (int k = 0; k < K; ++k)
                    C[i * N + j] += A[i * K + k] * B[k * N + j];
        benchmark::DoNotOptimize(C.data());
    }
}
BENCHMARK(BM_MatMul)->Ranges({{16, 64}, {16, 64}, {16, 64}});

// ArgsProduct 笛卡尔积
static void BM_Memset(benchmark::State& state) {
    int size = state.range(0), value = state.range(1);
    std::vector<char> buf(size, 0);
    for (auto _ : state) {
        std::memset(buf.data(), value, size);
        benchmark::DoNotOptimize(buf.data());
    }
}
BENCHMARK(BM_Memset)->ArgsProduct({{1024, 1048576}, {0, 255}});

// Apply 自定义
static void BM_Pow2(benchmark::State& state) {
    int n = state.range(0);
    volatile long sum = 0;
    for (auto _ : state) {
        for (int i = 0; i < n; ++i) sum += i * i;
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_Pow2)->Apply([](benchmark::internal::Benchmark* b) {
    for (int n = 3; n <= 30; n += 3) b->Args({1 << n});
});

BENCHMARK_MAIN();
