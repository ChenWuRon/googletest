# Google Benchmark 入门 — 第二课：写出可信的 Benchmark

## 目录

- [Benchmark 为什么容易测错](#benchmark-为什么容易测错)
- [DoNotOptimize](#donotoptimize)
- [ClobberMemory](#clobbermemory)
- [PauseTiming / ResumeTiming](#pausetiming--resumetiming)
- [Benchmark 最佳实践](#benchmark-最佳实践)
- [练习：Bad vs Good](#练习bad-vs-good)
- [速查表](#速查表)

---

## Benchmark 为什么容易测错

### 编译器优化

| 优化 | 效果 |
|------|------|
| 死代码消除 | 未使用的结果被删除，测到 0 ns |
| 常量折叠 | `1+1` 编译期算出 `2`，不在运行时执行 |
| 循环不变代码外提 | 把循环内不变的计算提到外面 |
| 函数内联 | 改变内存布局和调用开销 |

### CPU Cache

- L1: ~1 ns, L2: ~4 ns, L3: ~10 ns, 主存: ~100 ns
- 顺序访问 (stride=1)：硬件预取生效，接近 L1 速度
- 跳跃访问 (stride=256)：每次 cache miss，从主存加载，慢 5-10 倍
- 两次相同测试可能因缓存冷/热状态得出不同结论

### 初始化成本

循环内构造对象（vector、string 等）会引入大量分配/释放开销，掩盖目标逻辑。

### 内存分配不是确定性的

- `malloc` 可能触发 `brk` 或 `mmap`，耗时几十 ns 到几 µs
- 大对象分配在不同平台行为不同

### 为什么 `std::chrono` 不可信

```cpp
auto start = std::chrono::high_resolution_clock::now();
target_function();
auto end = std::chrono::high_resolution_clock::now();
```

- 单次测量噪声大
- 编译器可能把整段代码优化掉
- 时钟精度限制（几十 ns 的误差对纳秒级操作不可接受）
- 无法排除系统调度干扰

Google Benchmark 解决方式：自动跑足够多次使总时间 > 目标时间（默认 ~1s），取统计平均。

---

## DoNotOptimize

### 作用

告诉编译器"这个变量必须物化"，阻止死代码消除。

```
benchmark::DoNotOptimize(x);
```

编译后为 0 条 CPU 指令，只影响编译器行为。

### 错误示例

```cpp
static void BM_Wrong(benchmark::State& state) {
    int a = 7, b = 8;
    for (auto _ : state) {
        int c = a * b;  // c 从未被使用 → 编译器删掉
    }
}
// 结果：0.000 ns
```

### 正确示例

```cpp
static void BM_Correct(benchmark::State& state) {
    int a = 7, b = 8;
    for (auto _ : state) {
        int c = a * b;
        benchmark::DoNotOptimize(c);  // "c 必须存在"
    }
}
// 结果：~0.35 ns
```

### 常见错误

| 错误写法 | 问题 |
|---------|------|
| `DoNotOptimize(p)` 而不是 `DoNotOptimize(*p)` | 保护了指针地址，不是指向的值 |
| 初始化写在循环内 | 构造/赋值开销被计入计时 |
| `std::cout << x` 代替 `DoNotOptimize(x)` | I/O 开销远大于被测逻辑（µs vs ns） |

### 面试题

> Q: DoNotOptimize 有运行时开销吗？
> A: 没有。它只产生一个 asm volatile 空语句，不产生 CPU 指令。

---

## ClobberMemory

### 作用

声明"内存状态被修改了"，阻止编译器在两次调用之间做内存写入合并或重排。

```
benchmark::ClobberMemory();
```

### 何时需要

连续多次写入同一变量：

```cpp
x = 1;
x = 2;
x = 3;
x = 4;
// 编译器只保留 x = 4
```

```cpp
x = 1;
benchmark::ClobberMemory();
x = 2;
benchmark::ClobberMemory();
// 4 次写入全部保留
```

### 何时不需要

- 只关心单个结果 → 用 `DoNotOptimize`
- 被测代码调用了外部函数 → 编译器已假设外部函数改了内存

### DoNotOptimize vs ClobberMemory

| | DoNotOptimize(x) | ClobberMemory() |
|---|---|---|
| 作用对象 | 单个变量 | 整个内存 |
| 效果 | 阻止删除该值的读写 | 阻止跨语句的内存优化 |
| 典型场景 | 保留计算结果 | 模拟连续寄存器写入 |

---

## PauseTiming / ResumeTiming

### 用法

```cpp
state.PauseTiming();   // 暂停计时
// ... 不计时的准备代码 ...
state.ResumeTiming();  // 恢复计时
// ... 被测代码 ...
```

### 什么应该 Pause

- 数据准备（填充数组、构造输入）
- 结果验证
- 预热

### 什么不能 Pause

- 被测逻辑本身
- 极短操作（< 50ns），PauseTiming 自身开销会污染结果

### 示例：排序 Benchmark

```cpp
// 错误：构造和排序一起计时
for (auto _ : state) {
    std::vector<int> v(n);
    // fill v ...
    std::sort(v.begin(), v.end());
}

// 正确：排除构造开销
std::vector<int> v(n);
for (auto _ : state) {
    state.PauseTiming();
    // reset v ...
    state.ResumeTiming();
    std::sort(v.begin(), v.end());
}
```

---

## Benchmark 最佳实践

### 禁止清单

| ❌ 不要 | 原因 |
|---------|------|
| `printf` / `cout` | I/O 系统调用比被测逻辑慢几个数量级 |
| `sleep` | 让出 CPU，结果被调度噪声淹没 |
| 网络请求 | 不可重现，延迟比 CPU 高 10^6 倍 |
| 文件 IO | 测的是磁盘缓存，不是你的代码 |
| 循环内的随机数生成 | `mt19937` 一次 ~200 ns，可能占 50% |
| 不相关的逻辑 | 循环内每一行都影响计时 |

### 正确做法

```
构造/初始化 → 循环外
随机数据 → 预生成数组，循环内只读取
计时区域 → 只包含目标逻辑，其余用 PauseTiming 排除
结果 → 必须用 DoNotOptimize 保护
```

---

## 练习：Bad vs Good

### Bad Benchmark（包含 3 个错误）

```cpp
static void BM_Bad(benchmark::State& state) {
    int n = state.range(0);
    for (auto _ : state) {
        std::vector<int> v(n);               // ① 循环内构造
        for (int i = 0; i < n; ++i) v[i] = i;
        long sum = 0;
        for (int i = 0; i < n; ++i)
            sum += v[i] * v[i] + 1;           // ② 没保护 sum → 循环可被优化掉
        benchmark::DoNotOptimize(v.data());   // ③ 保护的指针，不是结果
    }
}
```

### Good Benchmark（修正版）

```cpp
static void BM_Good(benchmark::State& state) {
    int n = state.range(0);
    std::vector<int> v(n);
    for (int i = 0; i < n; ++i) v[i] = i;    // ✅ 初始化一次
    for (auto _ : state) {
        long sum = 0;
        for (int i = 0; i < n; ++i)
            sum += v[i] * v[i] + 1;
        benchmark::DoNotOptimize(sum);        // ✅ 保护结果
    }
}
```

### 运行结果（N=50000, -O2）

```
Benchmark              Time             CPU
-------------------------------------------
BM_Bad/50000       17955 ns        17955 ns     ← "快"但错
BM_Good/50000      38958 ns        38958 ns     ← 慢但正确
```

Bad 版本"快" 2.2 倍——因为求和循环被优化掉了，实际在测内存分配。如果只看数字选 BM_Bad，会被误导。

---

---

# 第三课：Benchmark 结果分析

## 输出结构

```
2026-06-29T15:30:30+08:00              ← 时间戳
Running /path/to/binary                ← 二进制路径
Run on (10 X 2000 MHz CPU s)           ← 硬件信息
Load Average: 0.11, 0.15, 0.17        ← 系统负载
-----------------------------------------------------
Benchmark           Time             CPU   Iterations
-----------------------------------------------------
BM_IntAdd       0.317 ns        0.317 ns   1000000000
```

| 列 | 含义 | 计算方式 |
|----|------|---------|
| `Time` | Wall-Clock 真实经过时间 | 总耗时 ÷ Iterations |
| `CPU` | CPU 实际执行时间 | CPU 总耗时 ÷ Iterations |
| `Iterations` | 框架自动选定的循环次数 | 自适应算法决定 |

## Iterations 自适应算法

```
初始化 iteration = 1
   ↓
执行循环，测量总耗时
   ↓
总耗时 ≥ min_time (默认 ~1s)？
  ├── 是 → 记录平均时间 = 总耗时 / iterations
  └── 否 → iterations *= 2，重测
```

- 框架有上限 1,000,000,000 次，防止无限循环
- 两次运行 Iterations 可能不同（系统负载波动）
- 不要用固定值（如 `for 1000次`）替代自适应机制

Iterations 大小与单次耗时成反比：

| 单次耗时 | Iterations | 说明 |
|---------|-----------|------|
| ~0.3 ns | 1,000,000,000 | 达到上限（极快操作） |
| ~213 µs | ~3,300 | 正常 |
| ~5 ms | ~135 | 慢操作 |

## Time vs CPU Time

| 场景 | Time | CPU | 关系 | 可信列 |
|------|------|-----|------|--------|
| 纯 CPU 计算 | 3.50 ms | 3.50 ms | Time = CPU | 均可 |
| sleep / I/O | 0.99 ms | 4.6 µs | **Time ≫ CPU** | **CPU** |
| 无竞争锁 | 4.55 ns | 4.55 ns | Time = CPU | 均可 |
| 混合计算+等待 | 0.96 ms | 49.7 µs | Time > CPU | **CPU** |

**核心：有 I/O/等待的 Benchmark 必须看 CPU 列。Time 被等待膨胀，不代表计算本身慢。**

## 吞吐量

| 函数 | 输出列 | 用途 |
|------|--------|------|
| `SetBytesProcessed(n)` | `bytes_per_second` | 数据处理：memcpy、压缩 |
| `SetItemsProcessed(n)` | `items_per_second` | 业务操作：请求处理、计算 |

框架自动计算：`吞吐量 = 总处理量 / 总耗时`

```
BM_Memcpy/1024          12.8 ns    bytes_per_second=74.37GiB/s
BM_Memcpy/1048576     17202 ns    bytes_per_second=56.77GiB/s
```

1KB 在 L1 cache (74 GiB/s)，1MB 受内存带宽限制 (57 GiB/s)。

## 复杂度拟合

| API | 作用 |
|-----|------|
| `state.SetComplexityN(n)` | 告知框架输入规模 |
| `->Complexity()` | 自动拟合并输出 BigO |

输出示例：
```
BM_StdSort_BigO          0.36 NlgN        ← O(NlogN)
BM_StdSort_RMS              1 %            ← 拟合误差 1%
BM_BubbleSort_BigO       0.94 N^2         ← O(N²)
BM_BubbleSort_RMS           1 %            ← 拟合误差 1%
```

拟合结果：

| 翻倍行为 | 复杂度 |
|---------|--------|
| N→2N, Time→×2 | O(N) |
| N→2N, Time→×2.1~2.2 | O(NlogN) |
| N→2N, Time→×4 | O(N²) |
| N→2N, Time→×8 | O(N³) |

## 结果分析 checklist

拿到 Benchmark 输出，按顺序检查：

1. **看 Iterations**：≥ 1000 才可信；10亿是上限，说明太快、可能有精度问题
2. **看 Time vs CPU**：差很多 → 有 I/O/等待，只看 CPU
3. **看比例**：N 翻倍时间怎么变？是否符合预期复杂度？
4. **看吞吐量**：缓存层级（L1/L2/L3/内存）会导致阶梯式变化
5. **看绝对数值**：ns 还是 µs？量级是否符合预期？

## 常见误判

| 错误 | 正确 |
|------|------|
| "快 3 倍，这个实现更好" | 先确认测的是 CPU 还是 Time，是不是在等 I/O |
| "Iterations 少，所以不稳定" | 每次 5ms 的操作跑 200 次就够 1s 了，200 次统计稳定 |
| "差 5%，这个版本更快" | 确认是否在测量噪声范围内（反复跑 5 次看方差） |
| "`unordered_map` 比 `map` 快" | 只在查找场景成立；有序遍历、稳定性场景反之 |

---

---

# 第四课：项目级 Google Benchmark

## Fixture（09_fixture.cpp）

### 为什么需要 Fixture

Benchmark 需要构建重型数据结构时，在循环内初始化会污染计时：

```cpp
// ❌ 错误：每次循环都构造 map（100K 次 insert 被计入）
for (auto _ : state) {
    std::unordered_map<int, int> m;
    for (int i = 0; i < 100000; ++i) m[i] = i;  // 占 99% 时间
    m.find(42);  // 这才是想测的
}
```

Fixture 写法：

```cpp
class BM_Map : public benchmark::Fixture {
public:
    std::unordered_map<int, int> m;
    std::vector<int> keys;

    void SetUp(const benchmark::State&) override {
        for (int i = 0; i < 100000; ++i) m[i] = i;
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
```

### 生命周期

```
BENCHMARK_F(BM_Map, Lookup):
  ┌─ BM_Map 对象构造
  ├─ SetUp()         ← 构建 10 万元素的 map（不计时）
  ├─ for (auto _ : state) { ... }  ← 只测查找
  ├─ TearDown()      ← clear()
  └─ 对象析构
```

每个 BENCHMARK_F 独立执行完整的 SetUp → 循环 → TearDown。

### 实际输出

```
BM_Map/Lookup           3.11 ns    ← 哈希表查找
BM_Map/InsertMiss       3.38 ns    ← 插入新 key（略慢）
BM_Map/EraseHit         266 ns     ← 删除（内存释放，慢 80 倍）
```

### 面试可能问

> Q: Fixture 和 PauseTiming 有什么区别？
> A: Fixture 适合"多个 benchmark 共享同一状态"；PauseTiming 适合单个函数内独立准备数据。

---

## 参数组合（10_parameter_combo.cpp）

### Args：手工指定

```cpp
BENCHMARK(BM_Sort)->Args({100})->Args({1000})->Args({10000});
```
```
BM_Sort/100                  422 ns
BM_Sort/1000                3949 ns
BM_Sort/10000              52225 ns
```

### DenseRange：连续整数

```cpp
BENCHMARK(BM_Loop)->DenseRange(10, 50, 10);
```
```
BM_Loop/10                  22.8 ns
BM_Loop/20                  44.3 ns
BM_Loop/30                  64.5 ns
BM_Loop/40                  85.3 ns
BM_Loop/50                   106 ns
```

### Ranges：多维度笛卡尔积

```cpp
BENCHMARK(BM_MatMul)->Ranges({{16, 64}, {16, 64}, {16, 64}});
```
```
BM_MatMul/16/16/16          1556 ns
BM_MatMul/64/64/64        149896 ns    ← M,N,K 全部 ×4
```

函数内用 `state.range(0)`、`state.range(1)`、`state.range(2)` 获取。

### ArgsProduct：扁平笛卡尔积

```cpp
BENCHMARK(BM_Memset)->ArgsProduct({{1024, 1048576}, {0, 255}});
```

生成 4 组：{1024,0} {1024,255} {1M,0} {1M,255}

### Apply：完全自定义

```cpp
BENCHMARK(BM_Pow2)->Apply([](auto* b) {
    for (int n = 3; n <= 30; n += 3) b->Args({1 << n});
});
```

生成 2³, 2⁶, 2⁹, ... 2³⁰。

### 选择指南

| 场景 | 用 |
|------|-----|
| 3-5 个手写值 | Args |
| 连续范围 | DenseRange |
| 指数增长 | Range |
| 多维参数全组合 | Ranges |
| 多组参数全部组合 | ArgsProduct |
| 任意自定义 | Apply |

---

## 多线程（11_multithread.cpp）

### 三种方式

```cpp
BENCHMARK(BM_Test)->Threads(4);           // 固定 4 线程
BENCHMARK(BM_Test)->ThreadRange(1, 8);    // 1, 2, 4, 8
BENCHMARK(BM_Test)->ThreadPerCpu();       // 每核一线程
```

### 输出解读

```
BM_Mutex_Single                4.43 ns    4.43 ns      ← 单线程
BM_Mutex_Multi/threads:4       2.36 ns    9.45 ns      ← Time×4≈CPU
BM_AtomicAdd/threads:1         4.80 ns    4.80 ns
BM_AtomicAdd/threads:2         2.43 ns    4.85 ns      ← Time 减半, CPU 不变
BM_AtomicAdd/threads:4         1.24 ns    4.95 ns
BM_AtomicAdd/threads:8        0.755 ns    5.07 ns
```

**核心规则：多线程 Time = 每线程平均，CPU = 所有线程总和。** `Time × threads ≈ CPU`

### 多线程难点

| 单线程 | 多线程 |
|--------|--------|
| 结果可复现 | 调度不确定 |
| 只受 CPU 频率影响 | 还受核数、缓存一致性影响 |
| 时间≈CPU | 时间可能被其他线程阻塞放大 |

---

## 自定义统计（12_counters.cpp）

### 三种 Counter 模式

```cpp
// 吞吐量（每秒速率）
state.counters["QPS"] = benchmark::Counter(
    state.iterations(), benchmark::Counter::kIsRate
);
// 输出: QPS=2.88G/s

// 平均延迟
state.counters["AvgLat"] = benchmark::Counter(
    latency, benchmark::Counter::kAvgIterations
);
// 输出: AvgLat=4.95u

// 累计值 + 1024 进制
state.counters["GB/s"] = benchmark::Counter(
    int64_t(state.iterations()) * n,
    benchmark::Counter::kDefaults,
    benchmark::Counter::OneK::kIs1024
);
// 输出: GB/s=38.79Gi
```

| 标志 | 含义 | 示例 |
|------|------|------|
| `kDefaults` | 总累计值 | `bytes=1048576` |
| `kIsRate` | 每秒速率 | `QPS=2.88G/s` |
| `kAvgIterations` | 每次迭代平均 | `AvgLat=4.95u` |
| `kAvgThreads` | 每线程平均 | `Threads=4` |
| `OneK::kIs1000` | 1000 进制 | `MB/s=58` |
| `OneK::kIs1024` | 1024 进制 | `GiB/s=38.79` |

---

## 真实案例对比（13_realworld.cpp）

### 1. vector vs list push_back

```
BM_VectorPush/100000     306366 ns    ← 快 4.8 倍
BM_ListPush/100000      1458495 ns
```

原因：list 每次 push 都 `new node`（堆分配），vector 连续内存偶尔扩容。
工作中：高频尾部插入用 vector。

### 2. map vs unordered_map insert

```
BM_MapInsert             41.4 ns
BM_UMapInsert            3.25 ns     ← 快 12.7 倍
```

原因：红黑树 O(logN) vs 哈希表 O(1)。
工作中：纯查找/插入用 unordered_map，需要有序遍历用 map。

### 3. shared_ptr vs unique_ptr

```
BM_SharedPtr             10.9 ns
BM_UniquePtr             9.58 ns     ← 快 14%
```

原因：shared_ptr 的原子引用计数有额外开销。
工作中：能用 unique_ptr 就不要用 shared_ptr。

### 4. sort vs stable_sort

```
BM_Sort/100000          4536630 ns     ← 快 12%
BM_StableSort/100000    5079456 ns
```

原因：stable_sort 需要保持相等元素顺序，需要额外内存和条件分支。
工作中：不需要稳定时用 sort。

### 5. reserve vs no reserve

```
BM_NoReserve/100000      217693 ns
BM_Reserve/100000        206742 ns     ← 快 5%
```

原因：reserve 避免多次扩容拷贝。重型元素差距更大。
工作中：知道大小时始终 reserve。

---

## 项目目录组织

### 推荐结构

```
project/
├── CMakeLists.txt
├── benchmark/
│   ├── CMakeLists.txt
│   ├── vector/        push_back_bench.cpp, sort_bench.cpp
│   ├── hashmap/       lookup_bench.cpp, insert_bench.cpp
│   ├── allocator/     malloc_bench.cpp
│   └── compression/   zstd_bench.cpp, lz4_bench.cpp
└── src/
```

### CI 配置

```yaml
steps:
  - run: cmake -B build -DCMAKE_BUILD_TYPE=Release
  - run: cmake --build build
  - run: ./build/bench --benchmark_out=result.json
  - uses: benchmark-action/github-action-benchmark@v1
    with:
      tool: 'googlecpp'
      output-file-path: result.json
      alert-threshold: '200%'
```

关键：Release 编译 + JSON 输出 + CI 阈值告警。

---

## 公司最佳实践

### 什么时候写

| 时机 | 原因 |
|------|------|
| 引入新数据结构/算法时 | 数据验证，不靠感觉 |
| 优化前 | 建立基线 |
| 优化后 | 验证是否真的变快了 |
| Code Review 有争议时 | 用数据说话 |

### Code Review 检查清单

```
❌ 循环内有 new/delete 或 vector 构造
❌ 没有 DoNotOptimize
❌ Debug 模式运行 benchmark
❌ 准备代码和被测代码混在一起计时
❌ 只看 Time 不看 CPU
✅ Release (-O2) 编译
✅ 循环外初始化
✅ DoNotOptimize 保护结果
✅ Iterations ≥ 1000
```

### 常见错误

| 错误 | 后果 |
|------|------|
| 没加 DoNotOptimize | 编译器删除代码，结果 0 ns |
| 循环内构造对象 | 构造开销污染计时 |
| -O0 编译 | 和线上不匹配 |
| 固定迭代次数 | 快代码精度不够 |
| 只看 Time 列 | 被 I/O 等待误导 |

### 让 Benchmark 更可信

1. **跑多次看方差**
   ```bash
   ./bench --benchmark_repetitions=10 --benchmark_display_aggregates_only=true
   ```

2. **Iterations 够大**
   - ≥ 1000：统计可信
   - 10亿（上限）：操作太快，可能有精度误差
   - < 100：受系统调度影响大

3. **同一台机器对比**
   - 不同 CPU 架构不能直接比绝对值，用相对比值

4. **先建基线后改代码**
   ```
   git stash && ./bench --benchmark_out=baseline.json
   git stash pop && ./bench --benchmark_out=change.json
   ```

### 维护原则

- Benchmark 也是代码，需要 review、维护、注释
- 重构后如果 benchmark 变快了，写清楚原因
- 定期检查被测函数是否改名导致 benchmark 失效
- CI 告警阈值设 20%，不要太松也不要太紧

## 速查表

| 目的 | 写法 |
|------|------|
| 阻止删除结果 | `benchmark::DoNotOptimize(result)` |
| 阻止写入合并 | `benchmark::ClobberMemory()` |
| 排除准备开销 | `state.PauseTiming()` / `state.ResumeTiming()` |
| 命令行调目标时间 | `--benchmark_min_time=0.5s` |
| 命令行调重复次数 | `--benchmark_repetitions=10` |
