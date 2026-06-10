# Resource Manager

基于 Linux cgroup 的资源编排框架。

## Overview

管理服务资源配额、自动发现目标进程、自动绑定资源策略、进程重启后自动恢复。

详见 `docs/` 目录：

- `docs/architecture.md` — 高层架构
- `docs/glossary.md` — 术语表
- `docs/adr/` — 架构决策记录
- `docs/plans/PLANS.md` — 实施计划

## Build

```bash
cmake -B build
cmake --build build
./build/resource-manager
```

## Test

```bash
cmake -B build -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Project Structure

```
resource-manager/
├── include/resource_manager/   # Public headers
│   ├── core/                   # ConfigDomain, ConfigParser, RuleBuilder
│   ├── mode/                   # Mode system
│   ├── state/                  # RuntimeState
│   ├── driver/                 # ICgroupDriver, ControlFileType
│   ├── discovery/              # IProcessDiscovery
│   ├── attach/                 # AttachEngine
│   ├── monitor/                # Monitor
│   └── error/                  # Error model
├── src/                        # Implementation
├── tests/                      # Unit tests (GTest)
├── configs/                    # Example configurations
├── docs/                       # Architecture & ADR docs
└── tools/                      # Utility scripts
```
