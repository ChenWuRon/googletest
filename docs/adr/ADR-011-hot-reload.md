# ADR-011 Hot Reload

Status: Accepted

Date: 2026-06-10

Authors: Project Team

---

# Context

配置文件可能在运行时发生变化。

Resource Manager 必须支持在不重启的情况下应用新的配置。

如果每次变更都重启：

* 服务中断
* 资源绑定丢失
* 恢复流程重新执行

但如果所有变更都热更新：

* 某些变更可能导致状态不一致
* 某些底层资源无法动态调整

因此需要明确：哪些允许热更新，哪些必须重启。

---

# Decision

## 热更新流程

```text
Load (检测文件变化)
    │
    ▼
Parse (解析新配置)
    │
    ▼
Build Tree (构建新 ConfigDomain Tree)
    │
    ▼
Diff (对比新旧 Tree)
    │
    ▼
Incremental Apply (增量应用变更)
```

## 允许热更新

| 变更类型 | 说明 |
|----------|------|
| 修改 item value | 如修改 cpu.max 的值 |
| 新增 item | 如新增 memory.max |
| 删除 item | 如删除不再需要的限制 |
| 新增 controller | 如新增 cpu 控制器配置 |
| 修改 match 规则 | 如扩展进程匹配范围 |

## 必须重启

| 变更类型 | 原因 |
|----------|------|
| 删除 group | 可能导致 cgroup 被删除，运行时状态无法处理 |
| 修改 group 名称 | 等价于删除 + 新增 |
| 修改 mode | 绑定方式变更，需要重新发现和 attach |
| cgroup 版本切换 | v1/v2 切换需要重建所有 cgroup |

## Diff 应用策略

| Diff 类型 | 处理方式 |
|-----------|----------|
| NodeAdded | 创建 cgroup / 写入控制文件 |
| NodeRemoved | 删除控制文件 / 删除 cgroup |
| NodeModified | 更新控制文件值 |
| NodeUnchanged | 跳过 |

---

# Consequences

采用本决策后：

优点：

* 大部分配置变更支持热更新，不中断服务
* Diff 驱动增量更新，效率高
* 明确区分热更新和重启场景
* 配置变更可观测

代价：

* 需要实现完整的 Diff 和增量 Apply 逻辑
* 某些变更必须重启，需要通知用户
* Diff 计算有额外开销

---

# Related ADR

ADR-003 Config Tree

ADR-005 Runtime State
