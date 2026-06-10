# ADR-007 Process Discovery

Status: Accepted

Date: 2026-06-10

Authors: Project Team

---

# Context

Resource Manager 需要找到目标进程，才能进行资源绑定。

目标进程由 Mode（ADR-004）和 MatchRule（ADR-002）描述：

```
group <name> {
    mode ...;
    match "<pattern>" { type <exact | prefix | wildcard>; }
}
```

Discovery 需要将这些声明式描述转换为具体的 PID 列表。

如果发现逻辑散落在各组件中：

* 每次 `/proc` 扫描的格式和策略不一致
* 匹配逻辑重复实现
* 难以扩展新的匹配后端（如 eBPF、netlink）
* 测试需要构造 `/proc` 目录

因此需要定义统一的 Process Discovery 接口。

---

# Decision

定义 `IProcessDiscovery` 接口，所有进程发现通过该接口完成。

---

## Interface — IProcessDiscovery

```cpp
class IProcessDiscovery {
public:
    virtual ~IProcessDiscovery() = default;

    // 根据匹配规则查找单个进程（期望唯一命中）
    virtual std::optional<ProcessInfo> findProcess(const MatchRule& rule) = 0;

    // 根据匹配规则查找所有匹配的进程
    virtual std::optional<std::vector<ProcessInfo>> findProcesses(const MatchRule& rule) = 0;

    // 查找指定 PID 的所有线程
    virtual std::optional<std::vector<ThreadInfo>> findThreads(int pid) = 0;
};
```

### ProcessInfo

```cpp
struct ProcessInfo {
    int pid;
    std::string comm;           // /proc/[pid]/comm
    std::string cmdline;        // /proc/[pid]/cmdline
    std::string exe;            // /proc/[pid]/exe (readlink)
    std::map<std::string, std::string> cgroup_paths;  // /proc/[pid]/cgroup
};
```

### ThreadInfo

```cpp
struct ThreadInfo {
    int tid;
    std::string comm;           // /proc/[pid]/task/[tid]/comm
};
```

---

## Supported Matching

### Exact

精确匹配进程 comm（`/proc/[pid]/comm`）。

| 规则 | 匹配 | 不匹配 |
|------|------|--------|
| `type exact; match "nginx"` | `nginx` | `nginx-worker`, `Nginx` |

### Prefix

按 comm 前缀匹配。

| 规则 | 匹配 | 不匹配 |
|------|------|--------|
| `type prefix; match "nginx"` | `nginx`, `nginx-worker` | `mynginx` |

### Wildcard

通配符匹配 comm 或 cmdline。支持 `*`（匹配任意字符）和 `?`（匹配单个字符）。

| 规则 | 匹配 | 不匹配 |
|------|------|--------|
| `type wildcard; match "nginx-*"` | `nginx-master`, `nginx-worker-1` | `nginx` |
| `type wildcard; match "python?script"` | `python1script` | `python12script` |

### 匹配字段

默认匹配 `comm`。可以通过配置扩展：

```yaml
# 当前支持
match "pattern" { type exact; }        # 匹配 comm
match "pattern" { type prefix; }       # 匹配 comm
match "pattern" { type wildcard; }     # 匹配 comm
```

未来扩展匹配 cmdline（见第 5 节）。

---

## 1. Why Discovery Is Separated from Monitor

### 核心职责差异

| 维度 | Discovery | Monitor |
|------|-----------|---------|
| 职责 | 找到进程 | 检测进程是否存活 |
| 操作 | 扫描 `/proc` 目录 | 检查 PID 是否存在 |
| 频率 | 启动时 + 恢复时 | 定期（每秒） |
| 产出 | ProcessInfo | 状态变化事件 |
| 匹配 | 需要完整的匹配引擎 | 只需要 PID 存在性检查 |

### 分离的好处

```
分离前（耦合）:
Monitor 既做定时检测又做进程扫描
  → 扫描 /proc 有 I/O 开销，阻塞 Monitor 的检测循环
  → 匹配逻辑嵌入 Monitor 代码，无法复用
  → Recovery 触发扫描时无法独立控制

分离后（解耦）:
Monitor 只做检测
  → 检测到 PID 变化 → 通知 Discovery 重新扫描
  → Discovery 扫描 /proc → 返回新的 ProcessInfo
  → Monitor 拿到新结果 → 通知 AttachEngine
  → 每个组件职责单一，可独立测试
```

---

## 2. Why Monitor Cannot Scan /proc Directly

| 原因 | 说明 |
|------|------|
| 职责混淆 | Monitor 的职责是"检测变化"，不是"查找目标" |
| 冗余实现 | 匹配逻辑（exact/prefix/wildcard）在 Monitor 中重复实现 |
| I/O 开销 | Monitor 的检测循环需要低延迟（毫秒级），`/proc` 扫描是磁盘 I/O |
| 测试复杂度 | Monitor 的测试需要同时模拟 `/proc` 内容和 PID 生命周期 |
| 扩展障碍 | 如果将来引入 eBPF 或 netlink 发现后端，Monitor 需要修改 |

---

## 3. Thread Discovery Model

### 线程发现方法

```cpp
// 读取 /proc/[pid]/task/ 目录
// 对每个 [tid] 读取 comm
std::optional<std::vector<ThreadInfo>> findThreads(int pid);
```

### 线程文件系统布局

```
/proc/[pid]/
├── task/
│   ├── [tid1]/
│   │   └── comm
│   ├── [tid2]/
│   │   └── comm
│   └── ...
└── comm (主线程 comm = tid1 的 comm)
```

### 线程在 Attach 中的用途

| 场景 | 是否需要发现线程 |
|------|-----------------|
| 进程级资源限制 | 不需要。Attach PID 到 cgroup.procs 自动移动所有线程 |
| 线程级资源限制 | 需要。每个线程独立 Attach 到 cgroup.threads |
| 线程级观测 | 需要。统计线程数量、线程 comm 识别 |

### 线程与 TID

`findThreads` 返回的 TID 列表包含主线程 TID（等于 PID）。

---

## 4. Discovery Cache Strategy

### 4.1 当前策略：无缓存

每次调用 `findProcess()` / `findProcesses()` 都执行完整的 `/proc` 扫描。

理由：

| 理由 | 说明 |
|------|------|
| /proc 是伪文件系统 | 扫描开销远低于真实磁盘 I/O |
| 数据时效性敏感 | PID 随时可能变化，缓存导致误判 |
| 扫描频率低 | 仅在启动和 Recovery 时调用，不是高频路径 |

### 4.2 未来扩展：进程列表缓存

如果扫描频率升高（如大规模集群），可以引入进程列表缓存：

```
struct DiscoveryCache {
    std::vector<ProcessInfo> all_processes;    // 全量快照
    std::chrono::steady_clock::time_point at;  // 快照时间
    int ttl_ms;                                // 有效时间
};
```

| 操作 | 缓存行为 |
|------|----------|
| findProcess (single) | 读缓存，若过期则刷新 |
| findProcesses (batch) | 读缓存，若过期则刷新 |
| findThreads | 不缓存（直接读 `/proc/pid/task`） |

当前不实现。

---

## 5. Discovery Scalability Target

| 指标 | 目标 |
|------|------|
| 单次全量 `/proc` 扫描 | `< 10ms`（1000 进程） |
| findProcess 延迟 | `< 5ms` |
| findThreads 延迟 | `< 2ms`（100 线程） |
| 并发扫描 | 支持（`/proc` 读取无锁） |

### 优化手段

| 手段 | 说明 |
|------|------|
| 直接读取 /proc/[pid]/comm | 不解析 /proc/[pid]/status |
| 使用 `readdir()` 而非 `scandir()` | 避免动态内存分配 |
| `ProcessInfo` 惰性填充 | 优先只读 comm，cmdline 和 exe 按需读取 |

---

## 6. Error Behavior

### 6.1 错误码

| 场景 | ErrorCode |
|------|-----------|
| /proc 无法访问 | `ProcessDiscoveryFailed` |
| 没有进程匹配 | `ProcessNotFound` |
| 匹配到进程但在 Attach 前退出 | `ProcessExited` |

### 6.2 错误处理规则

```
findProcess / findProcesses
    │
    ├── /proc 不可读 → 返回 ProcessDiscoveryFailed
    ├── 扫描完成但无匹配 → 返回 ProcessNotFound
    └── 扫描完成有匹配 → 返回 ProcessInfo

findThreads
    │
    ├── PID 不存在 → 返回 ProcessNotFound
    └── 成功 → 返回 ThreadInfo 列表
```

### 6.3 空结果 vs 错误

| 情况 | 返回值 |
|------|--------|
| 扫描完成，确实没有匹配进程 | `nullopt`（非错误，正常业务结果） |
| 扫描失败（/proc 不可读） | `optional<Error>`（系统错误） |

Discovery 的调用方需要区分这两种情况。

---

## 7. Process Identity Definition

### 7.1 Identity 组成

进程身份由重启后不变的属性构成：

| 属性 | 来源 | 重启后 |
|------|------|--------|
| comm | `/proc/[pid]/comm` | 通常不变（受 cmdline 影响） |
| cmdline | `/proc/[pid]/cmdline` | 不变 |
| exe | 可执行文件路径 | 不变 |
| cgroup 路径 | `/proc/[pid]/cgroup` | 可能变化（如容器重建） |

### 7.2 Identity 用于 Recovery

```
进程启动时:
  PID = 1234, comm = "nginx", cgroup = "/system.slice/nginx.service"

进程重启后:
  PID = 5678, comm = "nginx", cgroup = "/system.slice/nginx.service"

Identity (comm + cgroup) 不变 → 认定为同一目标 → Reattach
```

### 7.3 Identity 匹配优先级

Recovery 中判断两个 ProcessInfo 是否代表同一进程：

```
1. 先比较 cgroup 路径（最可靠）
2. 再比较 comm（通常可靠）
3. 最后比较 exe 路径
```

---

# Consequences

采用本决策后：

优点：

* Discovery 职责单一：只负责扫描 `/proc` 和匹配
* 三种匹配模式覆盖常见发现需求
* Monitor 和 Discovery 解耦，各自独立演进
* ProcessInfo 包含完整的进程身份信息，支持 Recovery
* 无缓存策略使逻辑简单，数据始终新鲜
* 可扩展 eBPF / netlink 后端而不影响接口

代价：

* 每次 Discovery 执行完整 `/proc` 扫描，大规模场景有性能担忧
* 未实现进程列表缓存，高频扫描时优化空间有限
* 三种匹配模式需要维护对应的匹配引擎

---

# Related ADR

ADR-002 Config Grammar — match 关键字和匹配规则
ADR-004 Mode System — Mode 决定 Discovery 的扫描范围（service / namespace / entity）
ADR-005 Runtime State — Discovery 写入 RuntimeState 的 pid/tids/discovery_status
ADR-008 Recovery — Monitor 触发 Rediscovery
ADR-010 Attach Engine — AttachEngine 消费 Discovery 结果
