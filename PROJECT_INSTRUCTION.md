# Resource Manager — Project Instruction

## 1. Project Overview

Resource Manager 是一个基于 Linux cgroup 的资源编排框架。

**核心职责：**
- 管理资源配置
- 管理资源绑定
- 管理资源恢复
- 提供资源观测能力

**不负责：**
- 进程生命周期管理（属于 systemd）
- 服务编排、依赖管理
- 容器运行时（Docker、containerd 等）
- Kubernetes 组件
- CPU/NUMA 调度算法
- 安全、认证、用户管理
- Metrics 存储平台（Prometheus、Grafana）

详见：`docs/adr/ADR-001`

---

## 2. Architecture

### 2.1 Components

```
Configuration
      │
      ▼
Config Parser ──▶ Rule Builder ──▶ Runtime State
      │                                │
      └──────────┬──────────┬──────────┘
                 ▼          ▼
        Cgroup Driver   Process Discovery   Monitor
                 │          │               │
                 └──────────┴───────────────┘
                             │
                             ▼
                       Linux Kernel
```

| Component | Responsibility | ADR |
|-----------|---------------|-----|
| Config Parser | 解析配置文件，生成 ConfigDomain Tree，语法校验 | ADR-002, ADR-003 |
| Rule Builder | ConfigDomain → Runtime Rules 转换 | — |
| Runtime State | 保存 PID/TID/Attach/Recovery 状态，与 ConfigState 分离 | ADR-005 |
| Cgroup Driver | 封装 `/sys/fs/cgroup` 操作，支持 v1/v2/mock | ADR-006 |
| Process Discovery | 扫描 `/proc`，支持 exact/prefix/wildcard | ADR-007 |
| Monitor | 检测 PID 变化，触发 Recovery | ADR-008 |
| Attach Engine | 编排 Attach 流程（createGroup → setValue → attachTask） | ADR-010 |

### 2.2 Startup Lifecycle

```
1. Load Configuration
2. Parse Configuration        → ConfigDomain Tree
3. Build Rules                → Runtime Rules
4. Create Cgroups             → Driver.createGroup()
5. Apply Resource Limits      → Driver.setValue()
6. Discover Processes         → ProcessDiscovery.findProcess()
7. Attach Processes           → AttachEngine
8. Start Monitoring           → Monitor
```

### 2.3 Recovery Lifecycle

```
Process Restart → PID Changed → Monitor Detects → Rediscover → Reattach → Restored
```

---

## 3. Config Grammar

三段式嵌套：`group → controller → item`

```
group <name> {
    mode <service | namespace | entity>;
    match <pattern> { type <exact | prefix | wildcard>; }

    controller <type> {
        item <name> = <value>;
    }
}
```

**命名规则：**
- group: 小写字母 + 连字符（如 `web-server`）
- controller: 小写字母，对应内核控制器名
- item: 小写字母 + 点号（如 `cpu.max`）

**校验规则：**
- group 名称唯一
- controller 类型合法（内核支持）
- item 合法（对应 controller 的有效控制文件）
- value 格式合法
- mode 必须指定
- match 不能为空

详见：`docs/adr/ADR-002`

---

## 4. ConfigDomain Tree

```
ROOT
 └── GROUP
      └── CONTROLLER
           └── ITEM
```

**路径访问：** `/groups/<name>/controllers/<type>/items/<name>`

**Diff 类型：** NodeAdded, NodeRemoved, NodeModified, NodeUnchanged

**遍历方式：** 深度优先、广度优先、路径查询、条件查询

详见：`docs/adr/ADR-003`

---

## 5. Mode System

```cpp
struct Mode {
    ServiceType service;
    NamespaceType ns;
    EntityType entity;
};
```

**禁止：** `uint32_t mode;`

Mode 组合：service-only / namespace-only / entity-only / 任意两者组合。

详见：`docs/adr/ADR-004`

---

## 6. Runtime State

**ConfigState** — 期望状态（ConfigDomain Tree，只读）
**RuntimeState** — 实际状态（动态更新）

```cpp
struct RuntimeState {
    pid_t pid;
    vector<pid_t> tids;
    AttachState attach_state;      // Pending | Attached | Detached | Failed
    RecoveryState recovery_state;  // None | Detecting | Recovering | Recovered | Failed
    time_t last_discovery_time;
    string cgroup_path;
};
```

详见：`docs/adr/ADR-005`

---

## 7. Cgroup Driver — ICgroupDriver

```cpp
interface ICgroupDriver {
    Result createGroup(path);
    Result removeGroup(path);
    Result setValue(path, file, value);
    Result<String> getValue(path, file);
    Result attachTask(path, tid);
};
```

**实现类：**
- `CgroupV2Driver` — 默认，原生 cgroup v2
- `CgroupV1Driver` — v1 兼容层
- `MockCgroupDriver` — 单元测试

**原则：** 业务层禁止直接访问 `/sys/fs/cgroup`。

详见：`docs/adr/ADR-006`

---

## 8. Process Discovery — IProcessDiscovery

```cpp
interface IProcessDiscovery {
    Result<Vec<ProcessInfo>> findProcess(pattern);
    Result<Vec<ThreadInfo>> findThreads(pid);
};
```

**匹配模式：** exact / prefix / wildcard

详见：`docs/adr/ADR-007`

---

## 9. Controller Model — ControlFileType

```cpp
struct ControlFileType {
    string name;
    ValueType value_type;  // String | Quota | Size | Weight | Bitmap | List | Int | Enum
    int version;           // 1 | 2
    string default_value;
};
```

**内置控制器：** cpu(Quota/Weight), memory(Size), cpuset(Bitmap/Enum), io(Weight), pids(Int)

详见：`docs/adr/ADR-009`

---

## 10. Attach Engine

```
Discovery → AttachEngine → Driver
                 │
                 ├── 1. createGroup()
                 ├── 2. setValue() × N
                 ├── 3. attachTask(PID)
                 └── 4. attachTask(TID) × N
```

AttachEngine 负责编排和重试，Driver 负责底层操作。

详见：`docs/adr/ADR-010`

---

## 11. Recovery — Monitor + Discovery + AttachEngine

| Component | Responsibility |
|-----------|---------------|
| Monitor | 检测 PID 变化，触发恢复 |
| ProcessDiscovery | 重新扫描 `/proc`，发现新 PID |
| AttachEngine | 执行 Reattach |

**重试策略：** 最多 3 次，间隔 1s，总超时 10s。

详见：`docs/adr/ADR-008`

---

## 12. Hot Reload

```
Load → Parse → Build Tree → Diff → Incremental Apply
```

**允许热更新：** 修改/新增/删除 item、新增 controller、修改 match 规则
**必须重启：** 删除 group、修改 group 名称、修改 mode、切换 cgroup 版本

详见：`docs/adr/ADR-011`

---

## 13. Error Model

```cpp
enum class ErrorCode {
    ConfigParseError, ConfigValidationError, ConfigFileNotFound,
    InvalidMode, ModeMismatch,
    ControllerNotFound, ControllerNotSupported, InvalidControlValue,
    ProcessNotFound, ProcessDiscoveryFailed, ProcessExited,
    AttachFailed, AttachTimeout, DetachFailed,
    CgroupWriteFailed, CgroupReadFailed, CgroupCreateFailed, CgroupRemoveFailed, CgroupVersionMismatch,
    RecoveryFailed, RecoveryTimeout,
    InternalError, NotImplemented,
};
```

详见：`docs/adr/ADR-012`

---

## 14. Implementation Plan

### Phase 1: Foundation

| # | Task | ADR | Output |
|---|------|-----|--------|
| 1 | 项目初始化（CMake/Cargo、目录结构、CI） | ADR-001 | 可编译的空项目 |
| 2 | ConfigDomain Tree 数据结构 | ADR-003 | Tree 定义、路径访问、遍历 |
| 3 | Error 模型 | ADR-012 | ErrorCode、Error 结构 |
| 4 | ICgroupDriver 接口 + Mock | ADR-006 | 接口定义、Mock 实现 |
| 5 | IProcessDiscovery 接口 + Mock | ADR-007 | 接口定义、Mock 实现 |
| 6 | Mode 系统 | ADR-004 | Mode 结构、组合逻辑 |

### Phase 2: Configuration

| # | Task | ADR | Output |
|---|------|-----|--------|
| 7 | Config Parser | ADR-002 | 解析器、语法校验 |
| 8 | ConfigDomain Tree Builder | ADR-003 | 从解析结果构建树 |
| 9 | Controller Model 注册表 | ADR-009 | 内置控制器定义 |
| 10 | Diff 算法 | ADR-003 | Tree Diff、增量更新 |

### Phase 3: Runtime

| # | Task | ADR | Output |
|---|------|-----|--------|
| 11 | Process Discovery (/proc) | ADR-007 | exact/prefix/wildcard |
| 12 | Cgroup Driver v2 | ADR-006 | createGroup、setValue、attachTask |
| 13 | AttachEngine | ADR-010 | 流程编排、重试 |
| 14 | RuntimeState 管理 | ADR-005 | 状态维护、查询 |
| 15 | Recovery 流程 | ADR-008 | Monitor、Rediscovery、Reattach |

### Phase 4: Operations

| # | Task | ADR | Output |
|---|------|-----|--------|
| 16 | Hot Reload | ADR-011 | 配置监听、Diff Apply |
| 17 | Cgroup v1 Driver | ADR-006 | v1 兼容层 |
| 18 | 集成测试 | — | E2E |
| 19 | 性能基准测试 | — | Benchmarks |
| 20 | 文档完善 | — | 用户文档、API 文档 |

---

## 15. Design Principles

1. **Separation of Concerns** — 配置解析、资源管理、进程发现、监控恢复必须解耦
2. **Driver Abstraction** — 业务层禁止直接访问 `/sys/fs/cgroup`
3. **Runtime Awareness** — 配置状态与运行状态分离
4. **Recoverability** — 所有资源绑定都必须支持自动恢复
5. **Extensibility** — 新 Controller/Namespace/Discovery Backend 不修改核心框架

---

## 16. Development Rules

### 16.1 Architecture Compliance

- 所有实现必须遵循 `docs/architecture.md`、`docs/adr/*.md`、`docs/glossary.md`
- 如果实现与 ADR 冲突：**立即停止**，创建 `docs/conflicts/ADR-CONFLICT-REPORT.md`
- 不引入新架构概念。如需引入，必须先提交 ADR 并获批准

### 16.2 Coding Conventions

- 禁止直接操作 `/sys/fs/cgroup` — 必须通过 `ICgroupDriver`
- 禁止使用 `uint32_t mode` — 必须使用 `struct Mode`
- 所有错误必须使用 `ErrorCode` 枚举传递
- `ConfigState` 与 `RuntimeState` 必须分离
- Controller 控制文件必须通过 `ControlFileType` 抽象

### 16.3 File Structure

```
docs/
├── architecture.md           # 高层架构
├── glossary.md               # 术语表
├── adr/                      # 架构决策记录
│   ├── ADR-001 ~ ADR-012
├── plans/
│   └── PLANS.md              # 实施计划
├── conflicts/                # 冲突报告（按需创建）

src/                          # 源码
include/                      # 头文件
tests/                        # 测试
configs/                      # 示例配置
tools/                        # 工具
```

---

## 17. Glossary

| Term | Definition |
|------|-----------|
| Group | 一组资源规则的集合，包含进程匹配规则和控制器配置 |
| Controller | cgroup 控制器，如 cpu、memory、cpuset |
| Item | 控制器下的具体资源限制项，如 cpu.max |
| Mode | 用于绑定目标的三元组 (service, namespace, entity) |
| ConfigDomain Tree | 配置解析后生成的多叉树结构 |
| Attach Engine | 负责将进程绑定到 cgroup 的编排组件 |
| Monitor | 检测 PID 变化并触发恢复的组件 |
| ControlFileType | 控制文件的统一抽象模型 |
| Recovery | 进程重启后自动恢复资源绑定 |
| Hot Reload | 不重启进程的情况下更新配置 |

完整术语表：`docs/glossary.md`
