#include <benchmark/benchmark.h>

static void BM_Write_NoClobber(benchmark::State& state) {
    int x = 0;
    for (auto _ : state) {
        x = 1;
        x = 2;
        x = 3;
        x = 4;
        benchmark::DoNotOptimize(x);
    }
}
BENCHMARK(BM_Write_NoClobber);

static void BM_Write_Clobber(benchmark::State& state) {
    int x = 0;
    for (auto _ : state) {
        x = 1;
        benchmark::ClobberMemory();
        x = 2;
        benchmark::ClobberMemory();
        x = 3;
        benchmark::ClobberMemory();
        x = 4;
        benchmark::ClobberMemory();
        benchmark::DoNotOptimize(x);
    }
}
BENCHMARK(BM_Write_Clobber);

BENCHMARK_MAIN();
