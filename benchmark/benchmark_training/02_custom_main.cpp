#include <benchmark/benchmark.h>

static void BM_Empty(benchmark::State& state) {
    for (auto _ : state) {
    }
}
BENCHMARK(BM_Empty);

static void BM_Add(benchmark::State& state) {
    int a = 1, b = 2;
    for (auto _ : state) {
        int c = a + b;
        benchmark::DoNotOptimize(c);
    }
}
BENCHMARK(BM_Add);

int main(int argc, char** argv) {
    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
