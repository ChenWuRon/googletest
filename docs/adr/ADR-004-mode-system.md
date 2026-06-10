# ADR-004 Mode System

Status: Accepted

Date: 2026-06-10

Authors: Project Team

---

# Context

Resource Manager 需要一个机制将配置中的 group 绑定到真实的目标（服务、进程、线程）。

绑定面临的核心问题：

* 目标有多种身份——属于哪个 systemd 服务、在哪个 cgroup namespace 下、进程名叫什么
* 同一种身份不足以唯一确定目标——同一台机器可能有多个同名进程，但属于不同服务
* 绑定方式需要随部署模式变化——容器化 vs 非容器化、systemd 托管 vs 独立进程

如果没有显式的 Mode 系统，常见的劣化路径是：

* 用 `uint32_t mode` 枚举所有组合 — 组合爆炸，无法扩展
* 用字符串模式匹配 — 运行时开销高，语义不明确
* 将绑定逻辑硬编码在 Discovery 中 — 每新增一种部署模式就修改 Discovery

因此需要一个显式、可组合、可扩展的 Mode 系统。

---

# Decision

## Mode 定义

Mode 是一个三元组，通过三个独立维度描述目标身份：

```cpp
struct Mode {
    ServiceType   service;   // 服务维度
    NamespaceType ns;        // 命名空间维度
    EntityType    entity;    // 实体维度
};
```

三个维度彼此正交，每个维度可以独立取值为 `None` 或具体类型。

`None` 表示该维度不参与匹配（通配）。

### ServiceType

| Value | 含义 | 适用场景 |
|-------|------|----------|
| `None` | 不按服务匹配 | 独立进程、容器内进程 |
| `Systemd` | 匹配 systemd  unit 名称 | systemd 托管的服务 |
| `Custom` | 自定义服务标识 | 上层平台定义的服务名 |

### NamespaceType

| Value | 含义 | 适用场景 |
|-------|------|----------|
| `None` | 不按 namespace 匹配 | 非容器化部署 |
| `Cgroup` | 匹配 cgroup 路径 | 容器内进程、cgroup 分层 |
| `Custom` | 自定义 namespace | 用户定义的分组逻辑 |

### EntityType

| Value | 含义 | 适用场景 |
|-------|------|----------|
| `None` | 不按实体匹配 | 匹配 service 下的所有进程 |
| `ProcessName` | 匹配 `/proc/pid/comm` | 按进程名发现 |
| `CommandLine` | 匹配 `/proc/pid/cmdline` | 按完整命令行发现 |
| `ExecutablePath` | 匹配可执行文件路径 | 按二进制路径发现 |

---

## 使用示例

### app ourea domain

匹配 ourea 域下的所有进程：

```
Mode {
    service:  Custom("ourea"),
    ns:       None,
    entity:   None,
}
```

### app ourea main

匹配 ourea 服务中的主进程（comm = "ourea-main"）：

```
Mode {
    service:  Custom("ourea"),
    ns:       None,
    entity:   ProcessName("ourea-main"),
}
```

### app ourea thread

匹配 ourea 服务中的线程（线程 comm 匹配 "ourea-worker-*"）：

```
Mode {
    service:  Custom("ourea"),
    ns:       None,
    entity:   ProcessName("ourea-worker-*"),
}
```

### systemd nginx worker

匹配 nginx.service 下名为 "nginx-worker" 的进程：

```
Mode {
    service:  Systemd("nginx"),
    ns:       None,
    entity:   ProcessName("nginx-worker"),
}
```

### 容器化应用

匹配 cgroup namespace "/docker/abc123" 下进程名为 "app" 的进程：

```
Mode {
    service:  None,
    ns:       Cgroup("/docker/abc123"),
    entity:   ProcessName("app"),
}
```

---

## 1. Why Mode Exists

Mode 解决三个核心问题：

### 1.1 目标身份多维度

一个运行中的进程同时具有：

* 服务身份 — 由 systemd unit、容器编排、或上层平台分配
* 命名空间身份 — cgroup 路径、容器 namespace、或用户自定义 scope
* 进程身份 — comm、cmdline、可执行路径

任何一个维度单独使用都存在盲区：

| 只用 | 盲区 |
|------|------|
| ServiceType | 同一服务下多个子进程无法区分 |
| EntityType | 不同机器的同名进程无法区分 |
| NamespaceType | 同一 namespace 内不同类型进程无法区分 |

Mode 通过三个维度的组合消除盲区。

### 1.2 配置与匹配策略解耦

Mode 将"目标是谁"的描述独立于"如何找到目标"的实现。

```
group web-server {
    mode service;            # 声明式描述
    match "nginx" { ... }
}

# 无需在配置中写 /proc 扫描逻辑
# 无需在配置中写 cgroup 路径拼接
```

Discovery 组件根据 Mode 选择匹配算法，而不是在配置中硬编码匹配策略。

### 1.3 进程重启后恢复

Mode 在进程重启后仍然有效（服务名不变、namespace 不变、匹配模式不变），因此 Recovery 可以根据 Mode 重新发现新 PID。

```
PID 变化 → Mode 不变 → Rediscovery(Group.mode) → 找到新 PID → Reattach
```

如果使用 PID 或文件描述符作为绑定依据，进程重启后无法恢复。

---

## 2. Why Not Bitmask

### 被拒绝的设计

```cpp
enum class ModeFlag : uint32_t {
    Service     = 1 << 0,
    Namespace   = 1 << 1,
    Entity      = 1 << 2,
    ServiceNs   = Service | Namespace,
    ServiceEnt  = Service | Entity,
    All         = Service | Namespace | Entity,
};

uint32_t mode = ModeFlag::Service | ModeFlag::Entity;
```

### 拒绝理由

| 问题 | 说明 |
|------|------|
| 组合爆炸 | 每个维度有 N 种取值，bitmask 需要 2^(N1+N2+N3) 种组合才能覆盖 |
| 无法携带数据 | bitmask 只能表达"启用哪些维度"，无法表达维度内的具体取值（如服务名 "nginx"） |
| 扩展成本高 | 新增一个枚举值需要改枚举定义 -> 改位运算逻辑 -> 改所有 switch case |
| 可读性差 | `mode & ModeFlag::Service` 不如 `mode.service == Systemd` 直观 |
| 类型不安全 | `uint32_t` 可以赋任何值，没有编译器保护 |

### 对比

| 能力 | Mode 三元组 | uint32_t bitmask |
|------|-------------|------------------|
| 表达"ourea 服务" | service = Custom("ourea") | 新增 OureaService 枚举值 |
| 表达"ourea 下的 worker 线程" | service + entity 组合 | 新增 OureaServiceWorker 枚举值 |
| 扩展新维度 | 新增一个字段 | 新增一个 bit，需要位移计算 |
| 类型安全 | 强类型枚举 | 无（uint32_t） |

结论：bitmask 适用于"开关"类组合，不适用于"带参数的多维度身份描述"。

---

## 3. Why Not String Matching

### 被拒绝的设计

```yaml
mode: "service:nginx entity:worker"
# 或
mode: "nginx.worker"
```

### 拒绝理由

| 问题 | 说明 |
|------|------|
| 运行时解析开销 | 每次匹配需要解析字符串、分割字段、做模式匹配 |
| 无类型安全 | 拼写错误在运行时才能发现 |
| 缺乏结构化 | 字符串无法表达"service 维度启用, entity 维度启用, ns 维度不启用"这样的结构 |
| 组合逻辑隐式 | `"nginx worker"` 是 AND 还是 OR 语义？ |
| 难以序列化/反序列化 | 需要自定义 parser，且格式变更后无法向前兼容 |

### 对比

| 能力 | Mode 三元组 | String matching |
|------|-------------|----------------|
| 编译时校验 | 是（类型系统） | 否 |
| 组合语义 | 显式（三个独立字段） | 隐式（字符串格式约定） |
| 性能 | O(1) 枚举比较 | O(n) 字符串解析 + 匹配 |
| 向前兼容 | 新增字段不影响旧代码 | 格式变更需要兼容旧字符串 |

---

## 4. Future Extension Strategy

Mode 的扩展遵循"新增字段，不改已有字段"原则。

### 4.1 新增维度

如果需要新的匹配维度（如 ContainerType、UserID），只需在 Mode 中增加一个字段：

```cpp
struct Mode {
    ServiceType    service;
    NamespaceType  ns;
    EntityType     entity;
    ContainerType  container;   // 新增：未来扩展
};
```

不影响已有代码：

* 旧代码读到 `container = None`，行为不变
* 新代码检查 `container` 取值，做额外匹配
* 所有已有的 group 配置无需修改

### 4.2 新增枚举值

每个 Type 枚举可以新增值：

```cpp
enum class ServiceType {
    None,
    Systemd,
    Custom,
    Kubernetes,   // 新增
};
```

同样不影响已有代码——旧配置使用旧值，新配置使用新值。

### 4.3 不允许的扩展

以下扩展方式被禁止：

* 在 Mode 中加入 `void*` 或 `std::any` 承载任意数据 — 破坏类型安全
* 在 Mode 中加入 `std::function<bool(pid_t)>` 自定义匹配函数 — 不可序列化，不可比较
* 将 Mode 改回 `uint32_t` — 退化（见 Why Not Bitmask）

### 4.4 扩展影响评估

| 扩展类型 | 需要修改 | 不需要修改 |
|----------|----------|------------|
| 新增枚举值 | Type 枚举定义 | Mode 结构体、Discovery 逻辑（增量添加） |
| 新增维度 | Mode 结构体、匹配逻辑 | 所有已存在的配置、序列化格式（新字段默认 None） |
| 新增匹配后端 | Discovery 实现 | Mode 定义、Config Grammar |

---

## 5. Serialization Format

Mode 序列化为 JSON 对象。

### 5.1 JSON Schema

```json
{
    "service": {
        "type": "systemd" | "custom" | null,
        "value": "<string>"        // 仅当 type 不为 null 时存在
    },
    "namespace": {
        "type": "cgroup" | "custom" | null,
        "value": "<string>"
    },
    "entity": {
        "type": "process_name" | "command_line" | "executable_path" | null,
        "value": "<string>"
    }
}
```

### 5.2 示例

```json
// ourea 服务, 按进程名匹配 worker
{
    "service":    { "type": "custom", "value": "ourea" },
    "namespace":  null,
    "entity":     { "type": "process_name", "value": "ourea-worker" }
}
```

```json
// systemd nginx, 不限制具体进程
{
    "service":    { "type": "systemd", "value": "nginx" },
    "namespace":  null,
    "entity":     null
}
```

```json
// 无 service, cgroup namespace + 可执行路径
{
    "service":    null,
    "namespace":  { "type": "cgroup", "value": "/docker/abc123" },
    "entity":     { "type": "executable_path", "value": "/usr/bin/app" }
}
```

### 5.3 配置文件中的写法

配置文件使用 ADR-002 语法，Mode 写在 group 内：

```
group web-server {
    mode service;

    # 等价于 JSON:
    # { "service": { "type": "systemd", "value": "nginx" },
    #   "namespace": null,
    #   "entity": null }
}
```

### 5.4 比较规则

两个 Mode 相等当且仅当三个维度的 type + value 全部相等。

```
{service=Custom("ourea"),  ns=None, entity=None}
    ≠
{service=Custom("nginx"),  ns=None, entity=None}

{service=Systemd("nginx"), entity=ProcessName("worker")}
    ≠
{service=Systemd("nginx"), entity=ProcessName("master")}
```

---

## 6. Validation Rules

### 6.1 单个维度校验

| 规则 | 说明 |
|------|------|
| type 不能为空 | 维度启用时必须指定具体 type |
| type 为 None 时 value 必须为空 | None 表示不启用，不应携带 value |
| type 非 None 时 value 不能为空 | 启用的维度必须指定匹配目标 |
| value 格式由 type 决定 | Systemd 类型 → systemd unit 名；Entity 类型 → 匹配模式（支持 wildcard） |

### 6.2 组合校验

```
至少一个维度非 None
```

一个 Mode 中至少有一个维度的 type 不是 None。全 None 的 Mode 没有匹配价值，拒绝。

### 6.3 逻辑校验

| 场景 | 规则 |
|------|------|
| service = Systemd, entity = ProcessName | 合法：在指定 systemd 服务内匹配进程名 |
| service = None, entity = ProcessName | 合法：全局匹配进程名 |
| service = Systemd, ns = Cgroup | 不推荐但合法：两个维度相互约束 |
| service = Systemd, ns = Cgroup, entity = ProcessName | 合法：三个维度同时约束 |

### 6.4 互斥规则

没有互斥规则。任意维度的任意组合都是合法的。

三个维度的组合是 AND 语义：匹配一个目标必须同时满足所有非 None 的维度。

---

# Consequences

采用本决策后：

优点：

* 三个维度正交，表达能力= Product(Type values)，扩展新值无需修改结构
* 类型安全，编译器可发现拼写错误和类型误用
* 序列化格式结构化，易于和其他系统交互（JSON）
* Process Discovery 根据 Mode 选择匹配策略，而非在配置中硬编码
* Recovery 依赖 Mode 保持不变的特性重新发现进程
* 扩展新维度不破坏已有代码（新字段默认 None）

代价：

* 组合匹配逻辑需要实现三个维度的 AND 计算
* 配置文件中 Mode 的语法比单枚举值复杂
* 需要校验逻辑确保维度的 type 和 value 一致

---

# Related ADR

ADR-002 Config Grammar — group 中的 mode 关键字
ADR-003 Config Tree — GROUP 节点持有 Mode
ADR-007 Process Discovery — Discovery 根据 Mode 选择匹配算法
ADR-008 Recovery — Recovery 依赖 Mode 不变性重新发现 PID
