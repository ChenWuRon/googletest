# Implementation Plans

## Phase 1: Foundation

目标：搭建项目骨架，实现核心模型和接口定义。

| # | Task | ADR | 预计产出 |
|---|------|-----|----------|
| 1 | 项目初始化（CMake/Cargo、目录结构、CI） | ADR-001 | 可编译的空项目 |
| 2 | 实现 ConfigDomain Tree 数据结构 | ADR-003 | Tree 定义、路径访问、遍历 |
| 3 | 实现 Error 模型 | ADR-012 | ErrorCode、Error 结构 |
| 4 | 定义 ICgroupDriver 接口 | ADR-006 | 接口定义、Mock 实现 |
| 5 | 定义 IProcessDiscovery 接口 | ADR-007 | 接口定义、Mock 实现 |
| 6 | 实现 Mode 系统 | ADR-004 | Mode 结构、组合逻辑 |

## Phase 2: Configuration

目标：实现配置解析、校验和构建树。

| # | Task | ADR | 预计产出 |
|---|------|-----|----------|
| 7 | 实现 Config Parser | ADR-002 | 解析器、语法校验 |
| 8 | 实现 ConfigDomain Tree Builder | ADR-003 | 从解析结果构建树 |
| 9 | 实现 Controller Model 注册表 | ADR-009 | 内置控制器定义 |
| 10 | 实现 Diff 算法 | ADR-003 | Tree Diff、增量更新 |

## Phase 3: Runtime

目标：实现进程发现、Attach、Recovery 流程。

| # | Task | ADR | 预计产出 |
|---|------|-----|----------|
| 11 | 实现 Process Discovery（/proc 扫描） | ADR-007 | exact/prefix/wildcard 匹配 |
| 12 | 实现 Cgroup Driver（v2 版本） | ADR-006 | createGroup、setValue、attachTask |
| 13 | 实现 AttachEngine | ADR-010 | Attach 流程编排、重试 |
| 14 | 实现 RuntimeState 管理 | ADR-005 | 状态维护、查询 |
| 15 | 实现 Recovery 流程 | ADR-008 | Monitor、Rediscovery、Reattach |

## Phase 4: Operations

目标：实现热更新、监控、Cgroup v1 兼容。

| # | Task | ADR | 预计产出 |
|---|------|-----|----------|
| 16 | 实现 Hot Reload | ADR-011 | 配置监听、Diff Apply |
| 17 | 实现 Cgroup v1 Driver | ADR-006 | v1 兼容层 |
| 18 | 集成测试（全流程） | — | E2E 测试 |
| 19 | 性能基准测试 | — | Benchmarks |
| 20 | 文档完善 | — | 用户文档、API 文档 |

## Dependencies

```text
Phase 1 ──▶ Phase 2 ──▶ Phase 3 ──▶ Phase 4
  │                        │
  └── 项目骨架             └── 核心功能
  └── 接口定义             └── 流程编排
  └── 模型定义             └── 自动恢复
```

Phase 1 是所有后续阶段的前提。

Phase 2 和 Phase 3 可部分并行（配置解析与进程发现接口定义并行）。

Phase 4 依赖 Phase 2 和 Phase 3 完成。

## Risks

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| cgroup v1 与 v2 语义差异大 | 接口抽象不完整 | Phase 1 接口设计时考虑 v1 兼容 |
| /proc 扫描竞态条件 | 进程信息不准确 | 多次验证、处理 ENOENT |
| 热更新导致状态不一致 | 运行时错误 | Diff 仿真、先验证再 Apply |
| 高并发下 Attach 竞争 | 内核错误 | 重试 + 退避策略 |
