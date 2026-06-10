# ADR-012 Error Model

Status: Accepted

Date: 2026-06-10

Authors: Project Team

---

# Context

Resource Manager 的每个操作都可能失败。

如果没有统一的错误模型：

* 错误信息不一致
* 调用方无法统一处理
* 错误传播路径混乱
* 日志和监控难以聚合

因此需要定义统一的错误体系。

---

# Decision

定义 `ErrorCode` 枚举，所有错误通过统一的错误模型传递。

## ErrorCode

```text
enum class ErrorCode {
    // 配置相关
    ConfigParseError,        // 配置文件解析失败
    ConfigValidationError,   // 配置校验失败
    ConfigFileNotFound,      // 配置文件不存在

    // Mode 相关
    InvalidMode,             // Mode 无效或不支持
    ModeMismatch,            // Mode 与运行时环境不匹配

    // Controller 相关
    ControllerNotFound,      // 控制器不存在
    ControllerNotSupported,  // 控制器在当前版本不支持
    InvalidControlValue,     // 控制文件值格式错误

    // 进程相关
    ProcessNotFound,         // 目标进程未找到
    ProcessDiscoveryFailed,  // 进程发现失败
    ProcessExited,           // 进程已退出

    // Attach 相关
    AttachFailed,            // Attach 操作失败
    AttachTimeout,           // Attach 超时
    DetachFailed,            // 分离操作失败

    // Cgroup 相关
    CgroupWriteFailed,       // cgroup 控制文件写入失败
    CgroupReadFailed,        // cgroup 控制文件读取失败
    CgroupCreateFailed,      // cgroup 目录创建失败
    CgroupRemoveFailed,      // cgroup 目录删除失败
    CgroupVersionMismatch,   // cgroup 版本不匹配

    // 恢复相关
    RecoveryFailed,          // 恢复流程失败
    RecoveryTimeout,         // 恢复超时

    // 内部错误
    InternalError,           // 内部错误
    NotImplemented,          // 功能未实现
};
```

## 错误结构

```text
struct Error {
    code: ErrorCode
    message: String
    source: String         // 错误来源（组件名）
    context: Map<String>   // 错误上下文（如 PID、路径等）
    cause: Option<Error>   // 嵌套错误
}
```

## 使用规范

| 组件 | 错误码前缀 |
|------|-----------|
| Config Parser | Config* |
| Mode System | InvalidMode, ModeMismatch |
| Controller Model | Controller*, InvalidControlValue |
| Process Discovery | Process* |
| AttachEngine | Attach*, Detach* |
| Cgroup Driver | Cgroup* |
| Recovery | Recovery* |

---

# Consequences

采用本决策后：

优点：

* 错误统一分类，便于定位问题
* 错误包含上下文信息，便于排查
* 支持错误嵌套，不丢失原始错误
* 日志和监控可以基于 ErrorCode 聚合

代价：

* 需要引入统一的错误类型定义
* 所有组件必须遵循错误规范

---

# Related ADR

所有 ADR
