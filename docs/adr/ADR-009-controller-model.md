# ADR-009 Controller Model

Status: Accepted

Date: 2026-06-10

Authors: Project Team

---

# Context

cgroup 控制器通过控制文件暴露资源限制能力。

每种控制器有一组控制文件：

| 控制器 | 文件 | 含义 |
|--------|------|------|
| cpu | `cpu.max` | CPU 配额 |
| cpu | `cpu.weight` | CPU 权重 |
| memory | `memory.max` | 硬限制 |
| memory | `memory.high` | 软限制 |
| cpuset | `cpuset.cpus` | CPU 亲和性 |
| io | `io.max` | IO 带宽 |
| pids | `pids.max` | 进程数限制 |

直接操作这些文件字符串会导致：

* 类型安全无法保证 — `cpu.max` 是配额对 `"100000 100000"`，`memory.max` 是大小值 `"1G"`，格式完全不同
* 格式验证分散 — 每个控制文件的合法值格式不同，验证逻辑遍布代码
* 版本差异渗透 — v1 中 cpu 限制是 `cpu.cfs_quota_us` + `cpu.cfs_period_us`，v2 中是 `cpu.max`，命名和格式都不同
* 扩展成本高 — 新增控制器需要在多处修改

因此需要统一抽象的 Controller Model。

---

# Decision

定义 Controller、Item、ControlFileType 三层模型，统一描述所有控制器。

---

## Model

### Controller

一个 Controller 对应一个内核 cgroup 控制器。

```cpp
struct Controller {
    std::string name;                          // "cpu", "memory", "cpuset"
    std::map<std::string, ControlFileType> items;  // 控制文件定义
    int min_version;                           // 1 | 2
};
```

### Item

一个 Item 对应一个控制文件。

```cpp
struct Item {
    std::string name;            // "cpu.max"
    std::string value;           // "100000 100000"
    ValueType value_type;        // Quota
};
```

### ControlFileType

控制文件的元数据定义。

```cpp
struct ControlFileType {
    std::string name;              // "cpu.max"
    ValueType value_type;          // Quota
    int version;                   // 该文件所属 cgroup 版本 (1 | 2)
    std::string default_value;     // 内核默认值
    std::string v1_equivalent;     // v1 中的等效文件（如 "cpu.cfs_quota_us"）
    ValidationRule validation;     // 值校验规则
};
```

### ValueType

```cpp
enum class ValueType {
    String,     // 任意字符串
    Quota,      // "max 100000" 或 "100000 100000"
    Size,       // "1G", "500M", "max"
    Weight,     // "1" – "10000"
    Bitmap,     // "0-3", "0,2"
    List,       // "8:16 rbps=1048576 wbps=1048576"
    Int,        // "1024", "max"
    Enum,       // "member" | "root"
};
```

### ValidationRule

```cpp
struct ValidationRule {
    std::string pattern;          // 正则表达式（可选）
    std::string min;              // 最小值（数值类型）
    std::string max;              // 最大值（数值类型）
    std::vector<std::string> enums; // 枚举值（Enum 类型）
    bool allow_max;               // 是否允许 "max" 关键字
};
```

---

## Built-in Controllers

### cpu (v2)

| Item | ValueType | 默认值 | 说明 |
|------|-----------|--------|------|
| `cpu.max` | Quota | `"max 100000"` | `$quota $period` |
| `cpu.weight` | Weight | `"100"` | 1 – 10000 |
| `cpu.weight.nice` | Int | `"0"` | -20 – 19 |
| `cpu.idle` | Int | `"0"` | 0 | 1 |
| `cpu.pressure` | String | `""` | 只读，压力信息 |

### memory (v2)

| Item | ValueType | 默认值 | 说明 |
|------|-----------|--------|------|
| `memory.max` | Size | `"max"` | 硬限制 |
| `memory.high` | Size | `"max"` | 软限制（throttling） |
| `memory.low` | Size | `"0"` | 低优先级保护 |
| `memory.min` | Size | `"0"` | 硬保护 |
| `memory.swap.max` | Size | `"max"` | 交换限制 |

### cpuset (v2)

| Item | ValueType | 默认值 | 说明 |
|------|-----------|--------|------|
| `cpuset.cpus` | Bitmap | `""` | CPU 核，如 "0-3" |
| `cpuset.mems` | Bitmap | `""` | 内存节点，如 "0" |
| `cpuset.cpus.partition` | Enum | `"member"` | "member" / "root" |

### io (v2)

| Item | ValueType | 默认值 | 说明 |
|------|-----------|--------|------|
| `io.max` | List | `""` | `$major:$minor rbps=$val wbps=$val` |
| `io.weight` | Weight | `"100"` | 1 – 10000 |

### pids (v2)

| Item | ValueType | 默认值 | 说明 |
|------|-----------|--------|------|
| `pids.max` | Int | `"max"` | 最大进程/线程数 |

---

## 1. Controller Abstraction

### 抽象层次

```
配置层（ConfigDomain Tree）
    │ ITEM("cpu.max") = "100000 100000"
    ▼
Controller Model 层（ControlFileType）
    │ 验证格式、转换 v1/v2 名称、提供默认值
    ▼
Driver 层（ICgroupDriver）
    │ write(path + file, value)
    ▼
内核层（/sys/fs/cgroup）
```

Controller Model 夹在配置层和 Driver 层之间，负责：

| 职责 | 说明 |
|------|------|
| 类型校验 | 根据 ValueType 验证值格式 |
| 默认值管理 | 配置未指定时使用内核默认值 |
| v1/v2 名称映射 | 同一语义在不同版本中的文件名不同 |
| 版本兼容 | 标记每个文件支持的 cgroup 版本 |

### 校验示例

```
输入: ITEM("cpu.max") = "abc def"
  → ControlFileType::value_type = Quota
  → ValidationRule: pattern = "^(max|\d+) \d+$"
  → 校验失败 → InvalidControlValue

输入: ITEM("memory.max") = "1G"
  → ControlFileType::value_type = Size
  → ValidationRule: pattern = "^(max|\d+[KMG]?)$"
  → 校验通过
```

---

## 2. Controller Registration

### 注册表

所有控制器定义集中注册在 `ControllerRegistry` 中。

```cpp
class ControllerRegistry {
public:
    static ControllerRegistry& instance();

    void register_controller(const Controller& ctrl);
    void register_control_file(
        const std::string& controller,
        const ControlFileType& type);

    const Controller* get_controller(const std::string& name) const;
    const ControlFileType* get_control_file(
        const std::string& controller,
        const std::string& item) const;

    void register_builtins();  // 注册内置的 cpu/memory/cpuset/io/pids

private:
    std::map<std::string, Controller> registry_;
};
```

### 启动时注册

```
Daemon 启动
    │
    ▼
ControllerRegistry::instance().register_builtins()
    │
    ├── register cpu     (cpu.max, cpu.weight, ...)
    ├── register memory  (memory.max, memory.high, ...)
    ├── register cpuset  (cpuset.cpus, cpuset.mems, ...)
    ├── register io      (io.max, io.weight, ...)
    └── register pids    (pids.max)
```

### 运行时查询

```cpp
// Config Parser 中校验 item 合法性
auto* type = ControllerRegistry::instance()
    .get_control_file("cpu", "cpu.max");
if (!type) {
    // 不支持的 item
    return ConfigValidationError;
}

// AttachEngine 中写入控制文件
auto* type = ControllerRegistry::instance()
    .get_control_file(controller_name, item_name);
driver->setValue(path, type->name, value);
```

---

## 3. v1/v2 Parameter Mapping

### 名称映射

| 语义 | v2 文件 | v1 文件 |
|------|---------|---------|
| CPU 配额 | `cpu.max` | `cpu.cfs_quota_us` + `cpu.cfs_period_us` |
| CPU 权重 | `cpu.weight` | `cpu.shares` |
| 内存硬限制 | `memory.max` | `memory.limit_in_bytes` |
| 内存软限制 | `memory.high` | `memory.soft_limit_in_bytes` |
| 内存通知 | `memory.events` | `memory.usage_in_bytes` |
| cpuset | `cpuset.cpus` | `cpuset.cpus`（同名） |

### 格式映射

| 语义 | v2 格式 | v1 格式 |
|------|---------|---------|
| CPU 配额 | `"100000 100000"`（单文件） | `cpu.cfs_quota_us = "100000"` + `cpu.cfs_period_us = "100000"`（双文件） |
| 内存大小 | `"1G"`（人类可读） | `"1073741824"`（字节） |

ControlFileType 的 `v1_equivalent` 字段用于名称映射。格式转换由对应版本 Driver 处理。

### CgroupV1Driver 的映射逻辑

```cpp
void CgroupV1Driver::setValue(
    const std::string& path,
    const std::string& file,
    const std::string& value)
{
    auto* type = registry_->get_control_file(controller_from_path(path), file);
    if (!type || type->v1_equivalent.empty()) {
        return write_file(path, file, value);  // 同名文件
    }

    if (file == "cpu.max") {
        // 拆分 "100000 100000" → quota + period
        // 写入 cpu.cfs_quota_us 和 cpu.cfs_period_us
        auto parts = split(value);
        write_file(path, "cpu.cfs_quota_us", parts[0]);
        write_file(path, "cpu.cfs_period_us", parts[1]);
    } else if (file == "memory.max") {
        // 转换 "1G" → "1073741824"
        write_file(path, "memory.limit_in_bytes", parse_size(value));
    }
    // ...
}
```

---

## 4. Validation Strategy

### 校验时机

```
配置加载时（ConfigParser）
  ├── Controller 是否存在 → ControllerNotFound
  ├── Item 是否在该 Controller 中定义 → InvalidControlValue
  └── Value 格式是否符合 ValueType → InvalidControlValue

运行时（AttachEngine setValue）
  └── Driver 写入前再次校验（防御性）
```

### 校验规则表

| ValueType | 校验规则 | 合法示例 | 非法示例 |
|-----------|----------|----------|----------|
| Quota | `^(max\|\d+) \d+$` | `"100000 100000"`, `"max 100000"` | `"abc"`, `""` |
| Size | `^(max\|\d+[KMG]?)$` | `"1G"`, `"500M"`, `"max"` | `"1GB"`, `"-1"` |
| Weight | `^([1-9]\d{0,3}\|10000)$` | `"100"`, `"1"`, `"10000"` | `"0"`, `"10001"`, `"abc"` |
| Bitmap | `^(\d+(-\d+)?)(,\d+(-\d+)?)*$` | `"0-3"`, `"0,2,4"` | `"a"`, `"-1"` |
| Int | `^(max\|\d+)$` | `"1024"`, `"max"` | `"-1"`, `"abc"` |
| Enum | must be in allowed list | `"member"` (if listed) | `"foo"` (not in list) |

### ControllerRegistry 校验方法

```cpp
std::optional<Error> ControllerRegistry::validate(
    const std::string& controller,
    const std::string& item,
    const std::string& value)
{
    auto* type = get_control_file(controller, item);
    if (!type) {
        return Error(InvalidControlValue, "unknown item", "ControllerRegistry");
    }
    // 根据 ValueType 和 ValidationRule 校验
    if (!match_pattern(value, type->validation)) {
        return Error(InvalidControlValue, "value format mismatch", "ControllerRegistry");
    }
    return std::nullopt;
}
```

---

## 5. Extension Strategy

### 新增控制器

```
1. 在 ControllerRegistry 中定义 Controller 结构
2. 调用 register_controller()
3. 无需修改 ConfigParser / AttachEngine / Driver
```

```cpp
// 新增 "misc" 控制器（cgroup v2 9.0+）
Controller misc_ctrl;
misc_ctrl.name = "misc";
misc_ctrl.min_version = 2;
misc_ctrl.items["misc.max"] = ControlFileType{
    .name = "misc.max",
    .value_type = ValueType::Int,
    .version = 2,
    .default_value = "max",
};

ControllerRegistry::instance().register_controller(misc_ctrl);
```

### 新增 Item 到已有控制器

```
1. 调用 register_control_file("cpu", new_type)
2. 配置文件中即可使用该 item
```

### 扩展原则

| 操作 | 影响范围 |
|------|----------|
| 新增 Controller | 仅 ControllerRegistry |
| 新增 Item | 仅 ControllerRegistry |
| 新增 ValueType | ControllerRegistry + ValidationRule |
| 修改 v1/v2 映射 | 仅 CgroupV1Driver |
| 修改内核接口 | 仅具体的 Driver 实现 |

**核心框架代码（ConfigParser、AttachEngine、Monitor）无需修改。**

---

# Consequences

采用本决策后：

优点：

* 所有控制文件的名称、类型、格式、版本、默认值集中管理
* ValueType + ValidationRule 提供统一校验入口
* v1/v2 差异通过名称映射和格式转换在 Driver 层解决
* 新增控制器或 item 无需修改核心框架
* 配置加载时即可发现 item 名称拼写错误，无需运行时才暴露

代价：

* 需要维护完整的控制器注册表
* 内置控制器的定义需要随内核版本更新
* v1/v2 的格式转换逻辑分散在 CgroupV1Driver 和 ControlFileType 之间

---

# Related ADR

ADR-002 Config Grammar — item 关键字使用 Controller Model 做校验
ADR-003 Config Tree — CONTROLLER 和 ITEM 节点对应 Controller Model
ADR-006 Cgroup Driver — Driver 根据 ControlFileType 确定写入路径和格式
