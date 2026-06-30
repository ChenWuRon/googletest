#include <benchmark/benchmark.h>

static void BM_Add_Wrong(benchmark::State& state) {
    for (auto _ : state) {
        int a = 1, b = 2, c = a + b;
    }
}
BENCHMARK(BM_Add_Wrong);

static void BM_Mul_Wrong(benchmark::State& state) {
    for (auto _ : state) {
        int a = 7, b = 8, c = a * b;
    }
}
BENCHMARK(BM_Mul_Wrong);

static void BM_Add_Correct(benchmark::State& state) {
    int a = 1, b = 2;
    for (auto _ : state) {
        int c = a + b;
        benchmark::DoNotOptimize(c);
    }
}
BENCHMARK(BM_Add_Correct);

static void BM_Mul_Correct(benchmark::State& state) {
    int a = 7, b = 8;
    for (auto _ : state) {
        int c = a * b;
        benchmark::DoNotOptimize(c);
    }
}
BENCHMARK(BM_Mul_Correct);

BENCHMARK_MAIN();
