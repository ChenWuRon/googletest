# ADR-007 Process Discovery

Status: Accepted

Date: 2026-06-10

Authors: Project Team

---

# Context

Resource Manager 需要找到目标进程，才能进行资源绑定。

进程发现需要扫描 `/proc` 文件系统。

如果扫描逻辑直接写在业务代码中：

* 扫描策略无法复用
* 难以测试
* 难以扩展新的发现方式

因此需要定义一个统一的 Process Discovery 接口。

---

# Decision

定义 `IProcessDiscovery` 接口，所有进程发现通过该接口完成。

## 接口定义

```text
interface IProcessDiscovery {
    findProcess(pattern: MatchRule): Result<Vec<ProcessInfo>>
    findThreads(pid: Int): Result<Vec<ThreadInfo>>
}
```

## 返回数据结构

```text
struct ProcessInfo {
    pid: Int
    comm: String          // 进程名
    cmdline: String       // 完整命令行
    cgroup_paths: Map     // cgroup 路径
}

struct ThreadInfo {
    tid: Int
    comm: String
}
```

## 匹配模式

| 模式 | 说明 | 示例 |
|------|------|------|
| exact | 精确匹配进程名 | `nginx` 精确匹配 nginx |
| prefix | 前缀匹配进程名 | `nginx-` 匹配 nginx-worker-1 |
| wildcard | 通配符匹配 | `nginx-*` 匹配所有 nginx 子进程 |

---

# Consequences

采用本决策后：

优点：

* 进程发现策略可扩展
* 支持三种匹配模式覆盖常见需求
* 易于测试（可 Mock /proc）
* 返回信息丰富（PID、comm、cmdline、cgroup 路径）

代价：

* 扫描 `/proc` 有性能开销
* 需要处理竞态条件（进程在扫描期间退出）

---

# Related ADR

ADR-004 Mode System

ADR-008 Recovery

ADR-010 Attach Engine
