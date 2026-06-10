# Resource Manager Architecture

## 1. Overview

Resource Manager 是一个基于 Linux cgroup 的资源编排框架。

目标：

* 管理服务资源配额
* 自动发现目标进程
* 自动绑定资源策略
* 进程重启后自动恢复
* 支持 cgroup v2
* 支持 systemd 服务

非目标：

* 容器编排
* Kubernetes
* Service Mesh
* 进程生命周期管理
* systemd 替代方案

---

## 2. Problem Statement

Linux cgroup 提供资源隔离能力，但存在以下问题：

1. 配置分散
2. 进程重启后资源绑定丢失
3. 线程级资源管理困难
4. 缺乏统一配置模型

Resource Manager 提供统一配置、统一发现和自动恢复能力。

---

## 3. High Level Architecture

```
             Configuration
                   │
                   ▼
            Config Parser
                   │
                   ▼
             Rule Builder
                   │
                   ▼
            Runtime State
                   │
    ┌──────────────┼──────────────┐
    ▼              ▼              ▼
```

Cgroup Driver  Process Discovery  Monitor
│              │              │
└──────────────┴──────────────┘
│
▼
Linux Kernel

---

## 4. Core Components

### Config Parser

职责：

* 解析配置文件
* 生成配置树
* 进行语法校验

输出：

* Configuration Model

---

### Rule Builder

职责：

* 配置模型转换
* 构建资源规则
* 构建任务规则

输出：

* Runtime Rules

---

### Runtime State

职责：

* 保存运行时状态
* 维护进程映射
* 保存资源绑定关系

---

### Process Discovery

职责：

* 扫描 /proc
* 发现目标进程
* 发现目标线程

---

### Cgroup Driver

职责：

* 创建 cgroup
* 设置资源限制
* Attach 任务

---

### Monitor

职责：

* 检测 PID 变化
* 重新发现进程
* 触发恢复流程

---

## 5. Startup Lifecycle

Startup 流程：

1. Load Configuration
2. Parse Configuration
3. Build Rules
4. Create Cgroups
5. Apply Resource Limits
6. Discover Processes
7. Attach Processes
8. Start Monitoring

---

## 6. Recovery Lifecycle

Process Restart
│
▼
PID Changed
│
▼
Monitor Detects Change
│
▼
Rediscover Process
│
▼
Reattach Resources
│
▼
Service Restored

---

## 7. Design Principles

### Separation of Concerns

配置解析、资源管理、进程发现、监控恢复必须解耦。

### Driver Abstraction

业务层禁止直接访问：

/sys/fs/cgroup

必须通过 Driver。

### Runtime Awareness

配置状态与运行状态分离。

### Recoverability

所有资源绑定都必须支持自动恢复。

### Extensibility

支持：

* 新 Controller
* 新 Namespace
* 新 Discovery Backend

而不修改核心框架。

---

## 8. Project Structure

docs/
src/
include/
tests/
configs/
tools/

详细目录结构见：

docs/adr/
docs/plans/

---


