# ADR-012 Error Model

Status: Accepted

Date: 2026-06-10

Authors: Project Team

---

# Context

Resource Manager 的每个操作都可能失败。

如果没有统一的错误模型：

* 错误信息不一致 — 有的返回 `bool`，有的返回 `int` errno，有的抛异常
* 调用方无法统一处理 — 每个调用者各自解析错误语义
* 错误传播路径混乱 — 底层 errno 穿透多层业务逻辑
* 日志和监控难以聚合 — 无法统一统计 "CgroupWriteFailed" 发生次数

因此需要定义统一的错误体系：所有错误通过 `ErrorCode` 枚举和 `Error` 结构传递。

---

# Decision

定义 `ErrorCode` 枚举、`Severity` 等级、`Error` 结构，所有组件统一使用该模型。

---

## ErrorCode

### Config

| Code | Severity | Recoverable | 说明 |
|------|----------|-------------|------|
| `ConfigFileNotFound` | ERROR | Yes | 配置文件路径不存在 |
| `ConfigParseError` | ERROR | Yes | 配置文件语法错误 |
| `ConfigValidationError` | ERROR | Yes | 配置语义校验失败 |

### Mode

| Code | Severity | Recoverable | 说明 |
|------|----------|-------------|------|
| `InvalidMode` | ERROR | Yes | Mode 取值无效 |
| `ModeMismatch` | ERROR | Yes | 运行时环境和 Mode 不匹配 |

### Controller

| Code | Severity | Recoverable | 说明 |
|------|----------|-------------|------|
| `ControllerNotFound` | ERROR | Yes | 配置中引用了未注册的控制器 |
| `ControllerNotSupported` | WARN | No | 内核不支持该控制器 |
| `InvalidControlValue` | ERROR | Yes | 控制文件值格式错误 |

### Process

| Code | Severity | Recoverable | 说明 |
|------|----------|-------------|------|
| `ProcessNotFound` | WARN | Yes | Discovery 未找到匹配的进程 |
| `ProcessDiscoveryFailed` | ERROR | Yes | /proc 扫描失败 |
| `ProcessExited` | INFO | Yes | 进程在 Attach 前已退出 |

### Attach

| Code | Severity | Recoverable | 说明 |
|------|----------|-------------|------|
| `AttachFailed` | ERROR | Yes | 写入 cgroup.procs 失败 |
| `AttachTimeout` | ERROR | Yes | Attach 操作超时 |
| `DetachFailed` | ERROR | Yes | 从 cgroup 移除进程失败 |

### Cgroup

| Code | Severity | Recoverable | 说明 |
|------|----------|-------------|------|
| `CgroupWriteFailed` | ERROR | Yes | 控制文件写入失败 |
| `CgroupReadFailed` | ERROR | Yes | 控制文件读取失败 |
| `CgroupCreateFailed` | ERROR | Yes | cgroup 目录创建失败 |
| `CgroupRemoveFailed` | ERROR | Yes | cgroup 目录删除失败 |
| `CgroupVersionMismatch` | ERROR | No | Driver 版本与内核不匹配 |

### Recovery

| Code | Severity | Recoverable | 说明 |
|------|----------|-------------|------|
| `RecoveryFailed` | ERROR | Yes | 恢复流程失败 |
| `RecoveryTimeout` | WARN | Yes | 恢复超时 |

### Internal

| Code | Severity | Recoverable | 说明 |
|------|----------|-------------|------|
| `InternalError` | ERROR | No | 未分类的内部错误 |
| `NotImplemented` | WARN | No | 功能尚未实现 |

---

## Severity

```cpp
enum class Severity {
    INFO,    // 不影响功能，仅信息
    WARN,    // 功能降级但仍可运行
    ERROR,   // 功能不可用
    FATAL,   // 进程终止
};
```

| Severity | 系统行为 | 示例 |
|----------|----------|------|
| INFO | 记录日志，继续运行 | ProcessExited |
| WARN | 记录日志，功能降级运行 | ControllerNotSupported |
| ERROR | 记录日志，重试或跳过 | AttachFailed |
| FATAL | 记录日志，终止进程 | ConfigParseError（启动时） |

### Severity 是 ErrorCode 的属性，不是调用者的判断

```cpp
// ErrorCode 自带 Severity
ErrorCode code = ErrorCode::AttachFailed;
Severity sev = code.severity();  // ERROR

// 调用者不需要判断严重程度
log(sev, "attach failed: pid={}", pid);
```

---

## Recoverability

```cpp
bool is_recoverable(ErrorCode code);
```

| Recoverable | 含义 | 处理方式 |
|-------------|------|----------|
| Yes | 重试可能成功 | 重试策略生效 |
| No | 重试必然失败 | 跳过或终止，不重试 |

### Recoverable 错误

```
ProcessNotFound    → 进程尚未启动，等下次 Discovery
ProcessExited      → 进程刚退出，等进程重启后恢复
AttachFailed       → 内核暂时繁忙，重试
CgroupWriteFailed  → 磁盘暂时错误，重试
ConfigParseError   → 配置文件修复后重试
```

### Non-Recoverable 错误

```
ControllerNotSupported  → 内核不支持，重试结果相同
CgroupVersionMismatch   → 版本不匹配，重试结果相同
InternalError           → 未知错误，重试无意义
NotImplemented          → 未实现，重试结果相同
```

---

## Error Structure

```cpp
struct Error {
    ErrorCode code;
    Severity severity;                    // 由 ErrorCode 决定
    std::string message;                  // 人类可读的描述
    std::string source;                   // 错误来源（组件名）
    std::map<std::string, std::string> context;  // 上下文
    std::unique_ptr<Error> cause;         // 根因（可选）

    Error(ErrorCode c, std::string msg, std::string src);
};
```

### context 示例

```cpp
Error(CgroupWriteFailed, "write cpu.max failed", "CgroupV2Driver")
    .with("path", "my_service/cpu")
    .with("file", "cpu.max")
    .with("value", "100000 100000")
    .with("errno", "28");  // ENOSPC
```

### cause 示例

```cpp
// 高层错误包裹底层错误
auto cause = std::make_unique<Error>(
    CgroupWriteFailed, "disk full", "CgroupV2Driver"
);
Error(AttachFailed, "cannot attach process", "AttachEngine")
    .with("pid", "1234")
    .with_cause(std::move(cause));
```

---

## 1. Recoverable Errors

### 重试决策链

```
操作失败 → 检查 ErrorCode.recoverable
    │
    ├── recoverable = No
    │   └── 跳过，记录 ERROR 日志，上报告警
    │
    └── recoverable = Yes
        └── 检查 retry_count
            ├── retry_count < max_retries
            │   └── 等待 retry_interval，重试
            └── retry_count >= max_retries
                └── 标记失败，记录 ERROR 日志，上报告警
```

### 可重试场景列表

| ErrorCode | 重试等待 | 典型场景 |
|-----------|----------|----------|
| ProcessNotFound | 1s | 进程尚未启动 |
| ProcessExited | 1s | 进程崩溃，等待重启 |
| AttachFailed | 500ms | 内核 busy |
| CgroupWriteFailed | 500ms | 磁盘压力 |
| CgroupCreateFailed | 500ms | 文件系统暂时不可用 |
| ConfigFileNotFound | 用户修复后信号触发 | 配置文件未就绪 |

---

## 2. Non-Recoverable Errors

### 不重试场景列表

| ErrorCode | 处理方式 | 原因 |
|-----------|----------|------|
| ConfigParseError | 保留旧配置 | 新配置有语法错误，修复前无法使用 |
| ConfigValidationError | 保留旧配置 | 新配置语义错误，修复前无法使用 |
| InvalidMode | 跳过该 group | Mode 定义错误 |
| ControllerNotSupported | 跳过该 controller | 内核不支持，当前环境无法解决 |
| CgroupVersionMismatch | 终止启动 | 版本不匹配需要重启 |
| InternalError | 上报告警 | 无明确恢复路径 |
| NotImplemented | 跳过 | 功能未就绪 |

### Non-Recoverable 错误是否需要继续运行

| 场景 | 行为 |
|------|------|
| 启动时 ConfigParseError | 终止（没有配置无法运行） |
| Hot Reload ConfigParseError | 保留旧配置，继续运行 |
| Hot Reload ControllerNotSupported | 跳过该 controller，继续运行 |
| 运行时 CgroupVersionMismatch | Driver 创建时已检测，不可能在运行时出现 |

---

## 3. Error Propagation

### 传播原则

1. **错误只向上传播，不横向跨越**。组件 A 的错误不会直接传给组件 C。
2. **每个组件在自己的边界捕获底层错误，包装成高层错误**。
3. **跨组件边界使用 Error 结构，不使用 errno 或异常**。

### 传播路径示例

```
Driver::setValue()
    │
    ├── open() 失败 → errno = EACCES
    ├── 包装为 Error(CgroupWriteFailed)
    │   .with("errno", "13")
    │
    ▼
AttachEngine::attach()
    │
    ├── 捕获 Driver 返回的 Error
    ├── 包装为 Error(AttachFailed)
    │   .with_cause(driver_error)
    │   .with("pid", "1234")
    │
    ▼
Monitor::check()
    │
    ├── 捕获 AttachEngine 返回的 Error
    ├── 记录日志
    └── 更新 RuntimeState.attach_status = Failed
```

### 不传播的场景

```
Driver::setValue() 写入单个 ITEM 失败
  → AttachEngine 记录错误
  → 继续写入下一个 ITEM
  → 不向上传播（非致命）
```

单个 ITEM 写入失败不影响其他 ITEM 和整个 Attach 流程的成功/失败状态。

---

## 4. Logging Interaction

### Error → Log 映射

```cpp
void log_error(const Error& err) {
    switch (err.severity) {
        case Severity::INFO:
            logger->info("{}: {} [{}]",
                err.code, err.message, err.source);
            break;
        case Severity::WARN:
            logger->warn("{}: {} [{}] | context={}",
                err.code, err.message, err.source, err.context);
            break;
        case Severity::ERROR:
            logger->error("{}: {} [{}] | context={}",
                err.code, err.message, err.source, err.context);
            break;
        case Severity::FATAL:
            logger->error("FATAL {}: {} [{}]",
                err.code, err.message, err.source);
            break;
    }
}
```

### 日志内容要求

每个 Error 的日志必须包含：

| 信息 | 来源 | 示例 |
|------|------|------|
| ErrorCode | err.code | `CgroupWriteFailed` |
| Message | err.message | `"write cpu.max failed"` |
| Source | err.source | `"CgroupV2Driver"` |
| Context | err.context | `path=my_service/cpu, file=cpu.max` |

### Error 统计（Metrics）

```cpp
// 按 ErrorCode 计数
metrics->counter("resource_manager.errors")
    .tag("code", error_code_string(err.code))
    .tag("source", err.source)
    .increment();
```

---

## 5. Retry Interaction

### ErrorCode → Retry Decision

```cpp
bool should_retry(const Error& err, int retry_count) {
    if (!is_recoverable(err.code)) {
        return false;  // 不可恢复，不重试
    }
    if (retry_count >= max_retries) {
        return false;  // 达到上限
    }
    return true;
}
```

### Retry 后的 Error 处理

```cpp
for (int i = 0; i < max_retries; i++) {
    auto err = driver->setValue(path, file, value);
    if (!err) return std::nullopt;  // 成功

    if (!should_retry(*err, i)) {
        // 不可恢复或超限
        return err;
    }

    std::this_thread::sleep_for(retry_interval);
}
// 所有重试耗尽
return last_error;
```

### 重试不改变 ErrorCode

重试耗尽后返回的 ErrorCode 与首次失败时相同。

`CgroupWriteFailed` 重试三次后仍然是 `CgroupWriteFailed`，不会变成 `RecoveryFailed`。

---

# Consequences

采用本决策后：

优点：

* 20+ 个 ErrorCode 覆盖所有组件的主要失败场景
* Severity 和 Recoverability 作为 ErrorCode 的内置属性，调用者无需自行判断
* 统一的 Error 结构携带上下文信息，日志和指标可以直接使用
* Error 嵌套支持根因追溯
* 重试决策基于 ErrorCode 的 recoverability 属性，规则集中管理
* 日志输出格式一致，便于自动化解析

代价：

* 需要维护 ErrorCode 枚举和 ErrorCode → (Severity, Recoverability) 的映射
* 所有组件必须遵循 Error 传递规范，代码审查时需要检查错误处理
* 嵌套 Error 的 `unique_ptr` 需要额外的内存分配

---

# Related ADR

所有 ADR — Error Model 是所有组件的共享基础设施
