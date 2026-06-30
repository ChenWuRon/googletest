#include <benchmark/benchmark.h>
#include <vector>

// 错误的 Benchmark
static void BM_Bad(benchmark::State& state) {
    int n = state.range(0);
    for (auto _ : state) {
        std::vector<int> v(n);               // ① 每次分配/释放内存
        for (int i = 0; i < n; ++i) v[i] = i;
        long sum = 0;
        for (int i = 0; i < n; ++i)
            sum += v[i] * v[i] + 1;           // ② sum 没被保护 → 这个循环可被删掉
        benchmark::DoNotOptimize(v.data());   // ③ 保护的指针，不是结果
    }
}
BENCHMARK(BM_Bad)->Arg(50000);

// 修正后的 Benchmark
static void BM_Good(benchmark::State& state) {
    int n = state.range(0);
    std::vector<int> v(n);
    for (int i = 0; i < n; ++i) v[i] = i;    // ✅ 只初始化一次
    for (auto _ : state) {
        long sum = 0;
        for (int i = 0; i < n; ++i)
            sum += v[i] * v[i] + 1;
        benchmark::DoNotOptimize(sum);        // ✅ 保护计算结果
    }
}
BENCHMARK(BM_Good)->Arg(50000);

int main(int argc, char** argv) {
    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}