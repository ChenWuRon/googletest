# ADR-004 Mode System

Status: Accepted

Date: 2026-06-10

Authors: Project Team

---

# Context

配置中的 group 必须绑定到实际的服务或进程。

绑定方式有多种：

* 按 service 名称绑定（如 nginx.service）
* 按 namespace 绑定（如 systemd scope）
* 按 entity 名称绑定（如进程名或命令行）

如果混用这些概念，会导致：

* 匹配逻辑混乱
* 难以扩展新的绑定方式
* 配置语义不清晰

因此需要定义一种显式的 Mode 系统。

---

# Decision

每个 group 必须指定一个 Mode。

Mode 是三元组：

```text
struct Mode
{
    ServiceType service;
    NamespaceType ns;
    EntityType entity;
};
```

## 字段说明

| 字段 | 类型 | 说明 |
|------|------|------|
| service | ServiceType | systemd 服务名或服务模式 |
| ns | NamespaceType | cgroup namespace 或自定义 namespace |
| entity | EntityType | 进程名、命令行、可执行路径 |

## 禁止的设计

以下设计被明确禁止：

```text
uint32_t mode;  // 禁止：整型枚举无法表达组合语义
```

## Mode 组合

Mode 支持组合查询：

| 组合方式 | 匹配逻辑 |
|----------|----------|
| service only | 匹配指定 systemd 服务的所有进程 |
| namespace only | 匹配指定 cgroup namespace 的所有进程 |
| entity only | 匹配指定进程名的所有进程 |
| service + entity | 匹配指定服务内符合进程名条件的进程 |
| namespace + entity | 匹配指定 namespace 内符合进程名条件的进程 |

---

# Consequences

采用本决策后：

优点：

* Mode 语义清晰，三种维度各司其职
* 支持灵活的组合匹配
* 易于扩展新的绑定维度
* 避免整型枚举的局限性

代价：

* 结构相对复杂
* 需要实现 Mode 的组合逻辑

---

# Related ADR

ADR-002 Config Grammar

ADR-007 Process Discovery
