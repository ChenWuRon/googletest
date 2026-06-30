#include <benchmark/benchmark.h>
#include <unordered_map>
#include <random>

class BM_Map : public benchmark::Fixture {
public:
    std::unordered_map<int, int> m;
    std::vector<int> keys;

    void SetUp(const benchmark::State&) override {
        for (int i = 0; i < 100000; ++i)
            m[i] = i;
        keys.resize(10000);
        std::mt19937 rng(42);
        for (auto& k : keys) k = rng() % 100000;
    }

    void TearDown(const benchmark::State&) override {
        m.clear();
    }
};

BENCHMARK_F(BM_Map, Lookup)(benchmark::State& state) {
    int i = 0;
    for (auto _ : state) {
        auto it = m.find(keys[i++ % keys.size()]);
        benchmark::DoNotOptimize(it);
    }
}

BENCHMARK_F(BM_Map, InsertMiss)(benchmark::State& state) {
    int i = 0;
    for (auto _ : state) {
        m[keys[i++ % keys.size()] + 200000] = 1;
    }
}

BENCHMARK_F(BM_Map, EraseHit)(benchmark::State& state) {
    int i = 0;
    for (auto _ : state) {
        state.PauseTiming();
        m[keys[i]] = 2;           // 确保 key 存在
        state.ResumeTiming();
        m.erase(keys[i++ % keys.size()]);
    }
}

BENCHMARK_MAIN();