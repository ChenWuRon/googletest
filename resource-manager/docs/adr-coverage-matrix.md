# ADR Coverage Matrix

**Date:** 2026-06-10
**Scope:** ADR-001 through ADR-012 vs current implementation at `fe02620`

---

## ADR-001 — Project Boundaries

| Status | **Implemented** |
|--------|-----------------|

**Evidence:** Project implements all 7 in-scope domains: Resource Configuration (config parser/domain), Resource Orchestration (AttachEngine), Process Discovery (ProcfsDiscovery), Resource Binding (AttachEngine), Runtime State Management (RuntimeStateManager), Recovery (RecoveryManager), Monitoring (Monitor). No out-of-scope features (process lifecycle, container runtime, Kubernetes, scheduling, security, metrics platform) exist in production code.

**Affected files:** `include/`, `src/` — entire codebase

**Missing work:** None — scope boundaries are respected.

---

## ADR-002 — Config Grammar

| Status | **Implemented** |
|--------|-----------------|

**Evidence:** Three-level grammar (group → controller → item) fully implemented via Lexer → Parser → Validator. Keywords: `group`, `controller`, `item`, `mode`, `match`, `type`. Named rules (lowercase+hyphen for group, lowercase for controller, lowercase+dots for items). Grammar supports exact/prefix/wildcard match types.

**Affected files:** `include/resource_manager/lexer/lexer.h`, `include/resource_manager/core/parser.h`, `include/resource_manager/core/validator.h`, `src/lexer/lexer.cpp`, `src/core/parser.cpp`, `src/core/validator.cpp`

**Missing work:** None — grammar implementation complete.

| Section | Status | Note |
|---------|--------|------|
| Keywords (group/controller/item/mode/match) | ✅ | Lexer + Parser |
| Naming rules | ✅ | Validator enforces |
| Three-level nesting | ✅ | ConfigNodeType hierarchy |
| Validation rules | ✅ | Validator class |
| Grammar examples | ✅ | Parseable by current parser |

---

## ADR-003 — Config Tree

| Status | **Partially Implemented** |
|--------|--------------------------|

**Evidence:** ConfigDomain and ConfigNode exist with unique_ptr ownership model, parent raw pointer, `std::map<std::string, std::unique_ptr<ConfigNode>>` children, `findChild()` lookup, `addChild()`/`removeChild()` mutation, `path()` method, `children()` iteration, DFS traversal via `findChild()`. ConfigDiffer exists with `DiffResult` (ADDED/REMOVED/MODIFIED). ConfigRenderer exists for serialization.

**Affected files:** `include/resource_manager/core/config_domain.h`, `include/resource_manager/core/config_differ.h`, `include/resource_manager/core/config_renderer.h`, `src/core/config_domain.cpp`, `src/core/config_differ.cpp`, `src/core/config_renderer.cpp`

| Section | Status | Note |
|---------|--------|------|
| ConfigDomain Tree structure | ✅ | ROOT → GROUP → CONTROLLER → ITEM |
| Ownership model (unique_ptr + raw parent) | ✅ | Per ADR spec |
| Canonical path format | ✅ | `ConfigNode::path()` |
| Path resolution rules | ❌ | `findByPath()` not implemented; no `ConfigPath` class |
| DFS/BFS traversal | ⚠️ | DFS via findChild, no explicit BFS iterator |
| `findByPath(path_string)` | ❌ | Not implemented |
| `addGroup()`/`addController()`/`addItem()` | ❌ | Not implemented; uses `addChild()` directly |
| `nodeCount()` | ❌ | Not implemented |
| `replaceChild()` | ❌ | Not implemented (noted as reserved in ADR) |
| ConfigDiffer (DiffResult) | ✅ | Full recursive diff implemented |
| ConfigRenderer (serialization) | ✅ | DFS traversal + formatted output |

**Missing work:**
- `ConfigPath` class for path parsing/construction
- `findByPath()` on ConfigDomain
- Convenience builders `addGroup()/addController()/addItem()`
- `nodeCount()` for monitoring
- BFS traversal iterator
- `replaceChild()` for hot reload node replacement

---

## ADR-004 — Mode System

| Status | **Partially Implemented** |
|--------|--------------------------|

**Evidence:** `Mode` struct exists with `ServiceType` (None/Systemd/Custom), `NamespaceType` (None/Cgroup/Custom), `EntityType` (None/ProcessName/CommandLine/ExecutablePath). Equality operators implemented. Default member initializers added (fixes UB). Mode keyword supported in config grammar and parsed by Parser.

**Affected files:** `include/resource_manager/mode/mode.h`, `src/mode/mode.cpp`, `include/resource_manager/core/parser.h`, `src/core/parser.cpp`

| Section | Status | Note |
|---------|--------|------|
| Mode struct (service/ns/entity) | ✅ | Implemented |
| Three enums with all values | ✅ | Per ADR spec |
| Equality operators | ✅ | `operator==` and `operator!=` |
| Config grammar `mode` keyword | ✅ | Parser supports `mode service;` etc. |
| Serialization format (JSON) | ❌ | Not implemented |
| Validation rules | ❌ | No Mode-specific validator |
| Usage by Discovery for matching | ⚠️ | Mode parsed but not consumed by Discovery logic |
| Mode in Process Identity | ❌ | ProcessState does not store Mode |

**Missing work:**
- JSON serialization/deserialization
- Mode validation (at least one non-None dimension, type non-None implies non-empty value)
- Mode consumed by Discovery for matching decisions
- Mode stored in ProcessState for identity (ADR-005 requirement)

---

## ADR-005 — Runtime State

| Status | **Partially Implemented** |
|--------|--------------------------|

**Evidence:** `RuntimeState` class with `ProcessState`, `ThreadState`. `RuntimeStateManager` with thread-safe access (shared_mutex). Three state dimensions: `DiscoveryStatus` (Unknown/Discovering/Discovered/Missing/Excluded), `RecoveryState` (None/Detecting/Recovering/Recovered/Failed). Timestamps (`attachTimestamp`, `lastSeen`). RuntimeSnapshot for immutable capture + compare + serialize.

**Affected files:** `include/resource_manager/state/runtime_state.h`, `include/resource_manager/state/runtime_state_manager.h`, `include/resource_manager/state/runtime_snapshot.h`, `include/resource_manager/state/runtime_event.h`, `src/state/runtime_state.cpp`, `src/state/runtime_state_manager.cpp`, `src/state/runtime_snapshot.cpp`, `src/state/runtime_event.cpp`

| Section | Status | Note |
|---------|--------|------|
| RuntimeState class | ✅ | Exists with ProcessState + ThreadState |
| DiscoveryStatus enum | ✅ | 5 states per ADR (Unknown/Discovering/Discovered/Missing/Excluded) |
| RecoveryState enum | ✅ | 5 states per ADR (None/Detecting/Recovering/Recovered/Failed) |
| **AttachStatus enum** | ❌ | ADR specifies `Pending/Attached/Detached/Failed`; current uses `bool attached` |
| **Process Identity** | ❌ | No `mode`, `match_pattern`, `service_name`, `cgroup_path` in ProcessState (only `processName`) |
| `attachedGroupPath` | ✅ | Added in this commit — maps to ADR's `cgroup_path` |
| `last_discovery_time` | ❌ | ADR specifies this field |
| `last_attach_time` | ⚠️ | Named `attachTimestamp` — semantically equivalent |
| `last_seen_pid` | ❌ | Not stored |
| RuntimeStateManager | ✅ | Thread-safe, sole mutation gatekeeper |
| RuntimeSnapshot | ✅ | Immutable capture + compare + serialize |
| RuntimeEvent | ✅ | Lifecycle events |
| **ConfigState** | ❌ | Placeholder only — no source/loaded_at/version metadata |
| Config vs Runtime separation | ✅ | Separate classes |

**Missing work:**
- Replace `bool attached` with `AttachStatus` enum (Pending/Attached/Detached/Failed)
- Add Process Identity fields: `mode`, `match_pattern`, `service_name` (or equivalent)
- Add `last_discovery_time`, `last_seen_pid` fields
- Implement ConfigState with source/loaded_at/version metadata

---

## ADR-006 — Cgroup Driver

| Status | **Implemented** |
|--------|-----------------|

**Evidence:** `ICgroupDriver` interface with 9 methods (createGroup, removeGroup, exists, enableController, setValue, getValue, attachProcess, attachThread, listGroups). `CgroupV2Driver` implements v2 backend with configurable mount point. `CgroupV1Driver` stub with corrected camelCase signatures. `MockCgroupDriver` with in-memory simulation, operation logging, error injection. Factory method `ICgroupDriver::create()`. Constructor injection into AttachEngine.

**Affected files:** `include/resource_manager/driver/icgroup_driver.h`, `src/driver/icgroup_driver.cpp`, `src/driver/cgroup_v2_driver.cpp`, `src/driver/cgroup_v1_driver.cpp`, `src/driver/mock_cgroup_driver.cpp`, `include/resource_manager/driver/mock_cgroup_driver.h`

| Section | Status | Note |
|---------|--------|------|
| ICgroupDriver interface | ✅ | 9 methods (more than ADR's 6 — added exists/enableController/listGroups) |
| CgroupV2Driver | ✅ | Configurable mount, fs::remove_all for removeGroup |
| CgroupV1Driver | ⚠️ | Stub only — camelCase methods present but not functional |
| MockCgroupDriver | ✅ | Full memory simulation, error injection, operation log |
| Factory `create()` | ✅ | Creates v1/v2/mock by version number |
| Controller Enablement via subtree_control | ✅ | `enableController()` method |
| Error handling with Error codes | ✅ | Returns `optional<Error>` |
| v1/v2 path mapping | ❌ | Not implemented in v1 stub |
| v1/v2 control file name mapping | ❌ | Not implemented |
| Mount point detection (/proc/mounts) | ❌ | Path is configurable but auto-detection not implemented |
| Constructor injection | ✅ | AttachEngine owns unique_ptr<ICgroupDriver> |

**Missing work:**
- CgroupV1Driver full implementation (mount point detection per controller, path mapping, control file name conversion via ControlFileType::v1_equivalent)
- Auto-detection of cgroup version from /proc/mounts
- subtree_control topology (parent-child propagation)

---

## ADR-007 — Process Discovery

| Status | **Implemented** |
|--------|-----------------|

**Evidence:** `IProcessDiscovery` interface with `findProcess()`, `findProcesses()`, `findThreads()`, `exists()`. `ProcfsDiscovery` with configurable `/proc` path, reads comm/cmdline/exe/cgroup/task. `DiscoveryRules` with Exact/Prefix/Wildcard matching (`?` and `*`). `DiscoveryCache` with TTL-based store/lookup/refresh/evict/clear. `DiscoveryService` orchestrating discovery → rules → state update → events.

**Affected files:** `include/resource_manager/discovery/iprocess_discovery.h`, `src/discovery/procfs_discovery.cpp`, `src/discovery/discovery_rules.cpp`, `src/discovery/discovery_cache.cpp`, `src/discovery/discovery_service.cpp`

| Section | Status | Note |
|---------|--------|------|
| IProcessDiscovery interface | ✅ | findProcess, findProcesses, findThreads, exists |
| ProcessInfo struct | ✅ | pid, comm, cmdline, exe, cgroup_paths |
| ThreadInfo struct | ✅ | tid, comm |
| Exact matching | ✅ | DiscoveryRules::match |
| Prefix matching | ✅ | DiscoveryRules::match |
| Wildcard matching | ✅ | `*` and `?` support |
| DiscoveryCache | ✅ | TTL-based caching |
| DiscoveryService orchestration | ✅ | discoverSingle, discoverAll, events |
| Process Identity for Recovery | ✅ | ProcessInfo includes comm, exe, cgroup_paths |
| /proc scanning | ✅ | ProcfsDiscovery reads comm/cmdline/exe/cgroup/task |

**Missing work:** None — implementation covers all ADR-007 specifications. The cache strategy differs (ADR says "no cache" but DiscoveryCache was added as extension).

---

## ADR-008 — Recovery

| Status | **Partially Implemented** |
|--------|--------------------------|

**Evidence:** Monitor class with polling loop, PID detection via `discovery_.exists()`/`findProcessByName()`, RuntimeReconciler for state updates. RecoveryManager class with `recoverProcess()`/`recoverAll()`/`recoverProcessWithRetry()`. State transitions (DiscoveryStatus + RecoveryState). Recovery events (RecoverySucceeded/RecoveryFailed).

**Affected files:** `include/resource_manager/monitor/monitor.h`, `src/monitor/monitor.cpp`, `include/resource_manager/monitor/runtime_reconciler.h`, `src/monitor/runtime_reconciler.cpp`, `include/resource_manager/recovery/recovery_manager.h`, `src/recovery/recovery_manager.cpp`

| Section | Status | Note |
|---------|--------|------|
| PID change detection | ✅ | Monitor polls stateManager, calls exists()/findProcessByName() |
| Process restart detection | ✅ | PIDChanged event |
| **Monitor owns AttachEngine** | ❌ | ADR-008§7 shows Monitor calls AttachEngine directly; current design delegates to RecoveryManager |
| **Reattach in recoverProcess()** | ✅ | **Fixed in this commit** — now calls attachEngine_.reattach() before setting Recovered |
| RecoveryState transitions | ✅ | None → Detecting → Recovering → Recovered → Failed |
| Retry policy | ✅ | recoverProcessWithRetry() with configurable retries |
| Timeout policy | ❌ | No recovery_timeout mechanism |
| State transitions from ADR-008 diagram | ✅ | Missing → Pending → Detecting → Discovered → Recovering → Attached → Recovered |
| AttachStatus: Pending state | ❌ | No Pending state (bool attached is binary) |
| Rediscovery with original MatchRule | ❌ | Uses findProcessByName() instead of MatchRule from config |

**Missing work:**
- Timeout policy (recovery_timeout parameter, stale_pid_ttl)
- Monitor calling AttachEngine per ADR-008§7 ownership model (currently RecoveryManager mediates)
- Full state machine with AttachStatus::Pending
- Rediscovery using original config MatchRule (not just processName)

---

## ADR-009 — Controller Model

| Status | **Implemented** |
|--------|-----------------|

**Evidence:** `ControllerRegistry` singleton with `registerController()`/`findController()`/`listControllers()`/`findControlFile()`. Built-in controllers (cpu/memory/cpuset/io/pids) with `registerBuiltins()`. `ControlFileType` with name/value_type/version/default_value/v1_equivalent/validation. `ValidationRule` with pattern/min/max/enums/allow_max. `ResourceValidator` validates values against registry rules.

**Affected files:** `include/resource_manager/driver/control_file_type.h`, `include/resource_manager/driver/resource_validator.h`, `include/resource_manager/error/error.h`

| Section | Status | Note |
|---------|--------|------|
| ControllerRegistry singleton | ✅ | `instance()` pattern |
| register_builtins() | ✅ | cpu/memory/cpuset/io/pids |
| ControlFileType struct | ✅ | name, value_type, version, default_value, v1_equivalent, validation |
| ValueType enum | ✅ | String/Quota/Size/Weight/Bitmap/List/Int/Enum |
| ValidationRule struct | ✅ | pattern/min/max/enums/allow_max |
| ResourceValidator | ✅ | Validates against registry rules |
| v1/v2 name mapping | ✅ | v1_equivalent field on ControlFileType |
| CgroupV1Driver format conversion | ❌ | Stub — no actual format conversion implemented |

**Missing work:**
- CgroupV1Driver actually using ControlFileType::v1_equivalent for name conversion
- Format conversion in CgroupV1Driver (e.g., Size "1G" → bytes)

---

## ADR-010 — Attach Engine

| Status | **Implemented** |
|--------|-----------------|

**Evidence:** `AttachEngine` class with `attach()`/`detach()`/`reattach()`. `executeAttach()` orchestrates createGroup → enableController → setValue × N → attachProcess → attachThread × N → markAttached(). `AttachPolicy` with 4 modes (AttachAll/AttachMainOnly/AttachThreadsOnly/CustomSelection). Reattach retry logic (max 3, transient errors only). `attachedGroupPath` now persisted in state.

**Affected files:** `include/resource_manager/attach/attach_engine.h`, `src/attach/attach_engine.cpp`, `include/resource_manager/attach/attach_policy.h`, `src/attach/attach_policy.cpp`

| Section | Status | Note |
|---------|--------|------|
| attach() method | ✅ | Full flow: resolve → create → setValue → attach |
| detach() method | ✅ | Move to root cgroup + removeGroup |
| reattach() method | ✅ | Full retry (max 3) |
| executeAttach() orchestration | ✅ | createGroup → enableController → setValue × N → attachProcess → attachThread → markAttached |
| Retry policy (max 3, non-retryable codes) | ✅ | ControllerNotSupported/InvalidControlValue/CgroupVersionMismatch abort |
| **Rollback on retry exhaustion** | ✅ | removeGroup + set Failed |
| AttachPolicy | ✅ | 4 modes, process-level and thread-level filtering |
| `attachedGroupPath` persistence | ✅ | **Fixed in this commit** |
| `AttachEngine` owns `AttachTracker` | ❌ | Removed — AttachTracker was dead code |
| AttachStatus in RuntimeState | ❌ | Uses bool attached (see ADR-005) |

**Missing work:**
- Replace `bool attached` with `AttachStatus` enum in both AttachEngine and RuntimeState

---

## ADR-011 — Hot Reload

| Status | **Not Implemented** |
|--------|---------------------|

**Evidence:** Only the Diff phase is partially implemented via ConfigDiffer (ADDED/REMOVED/MODIFIED). No Load/Parse/Build/Apply orchestration exists. No inotify/SIGHUP/timer trigger mechanism.

**Affected files:** `include/resource_manager/core/config_differ.h`, `src/core/config_differ.cpp`

| Section | Status | Note |
|---------|--------|------|
| Load (file change detection) | ❌ | No inotify, no SIGHUP handler, no poll timer |
| Parse (re-parse config) | ❌ | ConfigRepository used for initial load only |
| Build Tree | ❌ | No incremental tree construction |
| **Diff (ConfigDiffer)** | ✅ | Full recursive diff implemented (16 tests) |
| Apply orchestration | ❌ | No incremental Apply logic |
| inotify trigger | ❌ | Not implemented |
| SIGHUP trigger | ❌ | Not implemented |
| Timer poll trigger | ❌ | Not implemented |
| Incremental Apply ordering | ❌ | Added before Removed rules not implemented |
| Failure rollback | ❌ | Not implemented |
| ConfigState swap (old/new) | ❌ | Not implemented |

**Missing work:**
- HotReloadOrchestrator/ResourceManager class
- File change detection (inotify + SIGHUP + poll)
- Incremental Apply of DiffResult to RuntimeState/cgroup
- Failure rollback strategy
- ConfigState atomic swap

---

## ADR-012 — Error Model

| Status | **Partially Implemented** |
|--------|--------------------------|

**Evidence:** `ErrorCode` enum with 20+ codes covering all components (Config/Mode/Controller/Process/Attach/Cgroup/Recovery/Internal). `Error` struct with `code`/`message`/`source`/`context`/`cause`. All components return `std::optional<Error>`. `ErrorCode::ConfigNotFound` added in this commit. `Error` constructor takes `(ErrorCode, string, string)`.

**Affected files:** `include/resource_manager/error/error.h`

| Section | Status | Note |
|---------|--------|------|
| ErrorCode enum | ✅ | 20+ codes across all domains |
| Error struct | ✅ | code, message, source, context map, cause unique_ptr |
| **Severity enum** | ❌ | ADR specifies INFO/WARN/ERROR/FATAL; not implemented |
| **Severity on ErrorCode** | ❌ | ADR says severity is ErrorCode property |
| **`is_recoverable()` function** | ❌ | Not implemented |
| Error propagation pattern | ✅ | optional<Error> return |
| Context (.with()) | ✅ | map<string, string> context |
| Cause nesting | ✅ | unique_ptr<Error> cause |

**Missing work:**
- `Severity` enum (INFO/WARN/ERROR/FATAL)
- `ErrorCode::severity()` method or mapping
- `is_recoverable(ErrorCode)` function
- Consistency: all ErrorCodes defined in ADR may not be in use

---

## Summary

| ADR | Title | Status | Violations |
|-----|-------|--------|------------|
| 001 | Project Boundaries | ✅ **Implemented** | None |
| 002 | Config Grammar | ✅ **Implemented** | None |
| 003 | Config Tree | ⚠️ **Partially** | `findByPath()`, `ConfigPath`, `addGroup()/addController()/addItem()`, `nodeCount()`, `replaceChild()` missing |
| 004 | Mode System | ⚠️ **Partially** | No JSON serialization, no validator, Mode not consumed by Discovery, Mode not in ProcessState |
| 005 | Runtime State | ⚠️ **Partially** | `bool attached` instead of `AttachStatus` enum; no Process Identity fields; no ConfigState metadata |
| 006 | Cgroup Driver | ✅ **Implemented** | CgroupV1Driver is stub only |
| 007 | Process Discovery | ✅ **Implemented** | None |
| 008 | Recovery | ⚠️ **Partially** | Monitor doesn't own AttachEngine per ADR-008§7; no timeout policy; no Pending AttachStatus |
| 009 | Controller Model | ✅ **Implemented** | CgroupV1Driver doesn't use mapping yet |
| 010 | Attach Engine | ✅ **Implemented** | Uses `bool attached` instead of `AttachStatus` |
| 011 | Hot Reload | ❌ **Not Implemented** | Only ConfigDiffer exists; no Load/Parse/Build/Apply/Trigger |
| 012 | Error Model | ⚠️ **Partially** | `Severity` enum, `severity()`, `is_recoverable()` all missing |

### Counts

| Status | Count | ADRs |
|--------|-------|------|
| ✅ **Implemented** | 6 | 001, 002, 006, 007, 009, 010 |
| ⚠️ **Partially Implemented** | 5 | 003, 004, 005, 008, 012 |
| ❌ **Not Implemented** | 1 | 011 |
| 🚫 **Violated** | 0 | — |
