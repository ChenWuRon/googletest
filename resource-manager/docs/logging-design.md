# Logging Framework — Design

## Overview

ILogger / Logger 基于 ADR-012 Error Model，为 Resource Manager 所有组件提供统一日志能力。

## Interface — ILogger

```
ILogger          (纯虚接口)
  └── Logger     (具体实现)
```

```cpp
class ILogger {
    virtual void log(LogLevel, const string& message);
    virtual void trace(msg);
    virtual void debug(msg);
    virtual void info(msg);
    virtual void warn(msg);
    virtual void error(msg);
};
```

## Log Levels

| Level | Value | Default filter | Console output |
|-------|-------|---------------|----------------|
| TRACE | 0     | filtered      | stdout         |
| DEBUG | 1     | filtered      | stdout         |
| INFO  | 2     | **pass**      | stdout         |
| WARN  | 3     | pass          | stdout         |
| ERROR | 4     | pass          | stdout         |

`min_level` 控制最低输出级别，低于该级别的日志被丢弃。

## LoggerConfig

| Field | Default | Description |
|-------|---------|-------------|
| file_path | "" | 日志文件路径；空 = 不输出文件 |
| min_level | INFO | 最低日志级别 |
| console_output | true | 是否输出到 stdout |
| file_output | false | 是否输出到文件 |
| max_file_size | 0 | 旋转占位；0 = 不旋转 |
| max_backup_files | 0 | 旋转占位 |

## Format

```
[2026-06-10 14:30:00.123] [INFO] message text
```

## Rotation

`rotate()` 方法在当前实现中：
- `max_file_size == 0` → 无操作，返回 nullopt
- `max_file_size > 0` → 返回 `ErrorCode::NotImplemented`

Phase 4 将替换为真实旋转逻辑。

## Thread Safety

所有 `log()` 调用通过 `std::mutex` 互斥。

## Error Handling

日志写入失败不会终止程序，仅输出到 stderr。
错误使用 ADR-012 的 `Error` 结构传递。
