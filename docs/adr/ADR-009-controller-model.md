# ADR-009 Controller Model

Status: Accepted

Date: 2026-06-10

Authors: Project Team

---

# Context

cgroup 控制器通过控制文件暴露资源限制能力。

不同的控制文件有不同的：

* 名称（cpu.max、memory.max、cpuset.cpus）
* 类型（字符串、整型、枚举）
* 格式（单值、配额对、列表）
* 版本（v1 独有、v2 独有、通用）
* 默认值

如果直接操作控制文件字符串：

* 类型安全无法保证
* 格式验证分散在各处
* 版本差异渗透到业务层

因此需要统一抽象为 ControlFileType。

---

# Decision

定义 `ControlFileType` 模型，统一描述所有控制文件。

## ControlFileType

```text
struct ControlFileType {
    name: String          // 控制文件名，如 cpu.max
    value_type: ValueType // 值类型
    version: Int          // cgroup 版本 (1 / 2)
    default_value: String // 默认值
}
```

## ValueType

```text
enum ValueType {
    String,     // 普通字符串
    Quota,      // 配额对，如 "100000 100000"
    Size,       // 大小值，如 "1G"
    Weight,     // 权重值，如 "100"
    Bitmap,     // 位图，如 "0-3"
    List,       // 列表，如 "0,1,2,3"
    Int,        // 整型
    Enum        // 枚举
}
```

## 内置控制器定义

### cpu (v2)

| 控制文件 | 类型 | 默认值 |
|----------|------|--------|
| cpu.max | Quota | "max 100000" |
| cpu.weight | Weight | "100" |
| cpu.weight.nice | Int | "0" |
| cpu.pressure | String | "" |

### memory (v2)

| 控制文件 | 类型 | 默认值 |
|----------|------|--------|
| memory.max | Size | "max" |
| memory.high | Size | "max" |
| memory.low | Size | "0" |
| memory.min | Size | "0" |
| memory.swap.max | Size | "max" |

### cpuset (v2)

| 控制文件 | 类型 | 默认值 |
|----------|------|--------|
| cpuset.cpus | Bitmap | "" |
| cpuset.mems | Bitmap | "" |
| cpuset.cpus.partition | Enum | "member" |

### io (v2)

| 控制文件 | 类型 | 默认值 |
|----------|------|--------|
| io.max | String | "" |
| io.weight | Weight | "100" |
| io.pressure | String | "" |

### pids (v2)

| 控制文件 | 类型 | 默认值 |
|----------|------|--------|
| pids.max | Int | "max" |

## 管理方式

所有控制文件通过 ICgroupDriver 接口统一管理：

```text
driver.setValue(path, type.name, value);
```

禁止直接拼接路径和文件名。

---

# Consequences

采用本决策后：

优点：

* 控制文件统一抽象，类型安全
* ValueType 支持自动格式化与校验
* 版本差异在 Controller Model 层面处理
* 新增控制器只需添加定义，无需修改核心逻辑

代价：

* 需要维护完整的 Controller Model 注册表
* 需要确保上下兼容

---

# Related ADR

ADR-006 Cgroup Driver
