# Dependency Review Report

**Date:** 2026-06-10
**Scope:** All 12 modules (39 headers) under `include/resource_manager/`

---

## 1. Dependency Graph

```
                         ┌─────────────────────────────────────────────────────┐
                         │                   ORCHESTRATION                     │
                         │                                                     │
                         │   ┌──────────┐  ┌──────────┐  ┌──────────┐        │
                         │   │  MONITOR  │  │ RECOVERY  │  │  ATTACH   │        │
                         │   │           │  │           │  │           │        │
                         │   └─────┬─────┘  └─────┬─────┘  └─────┬─────┘        │
                         │         │               │              │             │
                         └─────────┼───────────────┼──────────────┼─────────────┘
                                   │               │              │
           ┌───────────────────────┼───────────────┼──────────────┼──────────┐
           │                       │               │              │          │
           │    ┌──────────────────┴───────┐   ┌────┴──────┐      │          │
           │    │   DISCOVERY_SERVICE      │   │   STATE   │      │          │
           │    │                          │   │           │      │          │
           │    └────────┬─────────────────┘   └────┬──────┘      │          │
           │             │                           │             │          │
           │    ┌────────┴────────┐        ┌─────────┴────────┐  │          │
           │    │ IPROCESS_       │        │ RUNTIME_STATE_   │  │          │
           │    │ DISCOVERY       │        │ MANAGER          │  │          │
           │    └────────┬────────┘        └─────────┬────────┘  │          │
           └─────────────┼───────────────────────────┼───────────┼──────────┘
                         │                           │           │
        ┌────────────────┼───────────────────────────┼───────────┼───────────┐
        │                │                           │           │           │
        │    ┌───────────┴──────────┐         ┌──────┴──────┐   │           │
        │    │      DRIVER          │         │    CORE     │   │           │
        │    │  (ICGROUP_DRIVER,    │         │ (CONFIG_    │   │           │
        │    │   CONTROLLER_        │         │  DOMAIN,    │   │           │
        │    │   REGISTRY, RESOURCE_│         │  PARSER,    │   │           │
        │    │   APPLIER, SNAPSHOT, │         │  DIFFER,    │   │           │
        │    │   VALIDATOR)         │         │  REPO,      │   │           │
        │    └──────────────────────┘         │  RENDERER)  │   │           │
        │                                     └──────┬──────┘   │           │
        └─────────────────────────────────────────────┼──────────┼───────────┘
                                                       │          │
                    ┌──────────────────────────────────┼──────────┼──────────┐
                    │           FOUNDATION              │          │         │
                    │   ┌──────┐ ┌──────┐ ┌──────┐     │          │         │
                    │   │ERROR │ │MODE  │ │LEXER │     │          │         │
                    │   └──────┘ └──────┘ └──────┘     │          │         │
                    │   ┌──────────┐ ┌───────────────┐  │          │         │
                    │   │COMMON    │ │CONFIG_DOMAIN  │  │          │         │
                    │   └──────────┘ └───────────────┘  │          │         │
                    │   ┌────────────┐ ┌──────────────┐ │          │         │
                    │   │RUNTIME_    │ │RUNTIME_      │ │          │         │
                    │   │STATE       │ │EVENT         │ │          │         │
                    │   └────────────┘ └──────────────┘ │          │         │
                    │   ┌─────────────────────┐         │          │         │
                    │   │RUNTIME_STATE_       │         │          │         │
                    │   │MACHINE              │         │          │         │
                    │   └─────────────────────┘         │          │         │
                    └───────────────────────────────────┼──────────┼─────────┘
                                                         │          │
                         ┌───────────────────────────────┴──────────┴─────────┐
                         │                  INTERFACES                        │
                         │   ┌────────────────────┐  ┌─────────────────────┐  │
                         │   │  IProcessDiscovery │  │   ICgroupDriver     │  │
                         │   └────────────────────┘  └─────────────────────┘  │
                         └────────────────────────────────────────────────────┘
```

### Direction key
- **Foundation**: zero internal dependencies
- **Interfaces**: depend only on `error/` and `config_domain.h`
- **Core / Driver / State**: depend on foundation + interfaces + each other in limited ways
- **Discovery Service**: depends on state (writes to `RuntimeStateManager`)
- **Orchestration**: depends on all lower layers

---

## 2. Module-by-Module Review

### 2.1 `state/` — Runtime State Management
| Metric | Value |
|--------|-------|
| Headers | 6 (`runtime_state.h`, `runtime_repository.h`, `runtime_snapshot.h`, `runtime_event.h`, `runtime_state_machine.h`, `runtime_state_manager.h`) |
| Internal deps | `runtime_repository.h` → `runtime_state.h` (same module only) |
| External deps | **None** (fully self-contained as a leaf module) |
| Cross-module callers | `monitor`, `recovery`, `discovery_service`, `runtime_reconciler`, `attach_engine`, `attach_policy`, `process_watcher` |

**Verdict:** Clean leaf module. `RuntimeStateManager` serves as the sole gatekeeper — all mutations go through it. `RuntimeState` and `RuntimeEvent` have zero external dependencies. **No issues.**

### 2.2 `discovery/` — Process Discovery
| Metric | Value |
|--------|-------|
| Headers | 5 (`iprocess_discovery.h`, `procfs_discovery.h`, `discovery_cache.h`, `discovery_rules.h`, `discovery_service.h`) |
| Internal deps | Same-module only |
| External deps | `iprocess_discovery.h` → `error/`, `config_domain.h`; `discovery_service.h` → `state/runtime_state_manager.h`, `state/runtime_event.h` |
| Key observation | **LAYERING CONCERN**: `discovery_service.h` includes `runtime_state_manager.h` (from `state/` module). This means discovery knows about state. At runtime, `discovery_service.cpp` writes to `stateManager_.registerProcess()`. |

**Verdict:** The `discovery_service` → `state` dependency is intentional but couples the layers. Discovery publishes events AND writes to state manager — two responsibilities. Consider whether `discovery_service` should only publish events and let a coordinator write to state.

### 2.3 `driver/` — Cgroup Driver & Resource Management
| Metric | Value |
|--------|-------|
| Headers | 7 (`icgroup_driver.h`, `cgroup_v2_driver.h`, `mock_cgroup_driver.h`, `control_file_type.h`, `resource_applier.h`, `resource_snapshot.h`, `resource_validator.h`) |
| External deps | `icgroup_driver.h` → `error/`; `control_file_type.h` → `config_domain.h`; `resource_applier.h` → `config_domain.h`; `resource_snapshot.h` → `config_domain.h`; `resource_validator.h` → `config_domain.h` |
| Cross-module calls from | `attach_engine` → `driver_` (cgroup ops); `resource_transaction` → `driver_` (getValue/setValue) |

**Verdict:** Clean. The dependency on `config_domain.h` from `core/` is appropriate — the driver reads the config tree to know which cgroups to create and what values to set. `resource_transaction` bidirectionally depends on `driver` (it calls `driver_.getValue/setValue` while also being in `core/` directory) — this is a **circular dependency at the directory level** but NOT at the class level (no cycles in the header graph: `resource_transaction.h` → `icgroup_driver.h`, `icgroup_driver.h` → `error.h` only).

### 2.4 `core/` — Config Infrastructure
| Metric | Value |
|--------|-------|
| Headers | 10 |
| External deps | `config_parser.h` → `error/`; `parser.h` → `lexer/token.h`; `config_repository.h` → `validator.h`; `rule_builder.h` → `error/`; `resource_transaction.h` → `driver/icgroup_driver.h`, `error/` |
| Key observation | `resource_transaction.h` lives in `core/` but depends on `driver/icgroup_driver.h`. This is **cross-module from core to driver** — `core` knows about `driver`. Also `config_repository.h` includes `validator.h` (same module, fine) |

**Verdict:** `resource_transaction` has an outward dependency to `driver/`, which inverts the layering (core ← driver). This is acceptable because `resource_transaction` is a utility that needs the driver for read-before-write rollback. Consider moving `resource_transaction` to `driver/` module.

### 2.5 `attach/` — Attach Engine
| Metric | Value |
|--------|-------|
| Headers | 3 (`attach_engine.h`, `attach_policy.h`, `attach_tracker.h`) |
| External deps | `attach_engine.h` → `core/config_domain.h`, `state/runtime_state.h`, `driver/icgroup_driver.h`, `error/`; `attach_policy.h` → `state/runtime_state.h` |
| Cross-module runtime calls | `ConfigNode::name()`, `children()`, `type()` (core); `RuntimeState::processState().pid`, `threads()`, `markAttached()`, `markDetached()` (state); `ICgroupDriver::attachProcess()`, `createGroup()`, `setValue()`, etc. (driver) |

**Verdict:** High dependency count (5). `AttachEngine` knows about config tree structure, process state internals, cgroup driver interface. This is inherent to its role (bridging config → cgroup for a given process). No abstraction leak — all dependencies are necessary.

### 2.6 `monitor/` — Monitoring
| Metric | Value |
|--------|-------|
| Headers | 3 (`monitor.h`, `process_watcher.h`, `runtime_reconciler.h`) |
| External deps | `monitor.h` → `state/runtime_state_manager.h`, `discovery/discovery_service.h`, `state/runtime_event.h`; `process_watcher.h` → `state/runtime_snapshot.h`, `state/runtime_state_manager.h`, `discovery/iprocess_discovery.h`; `runtime_reconciler.h` → `state/runtime_state_manager.h`, `error/` |

**Verdict:** `monitor` sits at the top of the layering which is appropriate. It depends on `state`, `discovery`. No direct dependency on `driver/` or `attach/` — correct separation of concerns. **Clean.**

### 2.7 `recovery/` — Recovery Manager
| Metric | Value |
|--------|-------|
| Headers | 1 (`recovery_manager.h`) |
| External deps | `state/runtime_state_manager.h`, `discovery/discovery_service.h`, `attach/attach_engine.h`, `state/runtime_event.h`, `error/` |
| **Usage gap** | `attachEngine_` is initialized in the constructor but **never called** in any method. The recovery flow skips `AttachEngine::reattach()`. |

**Verdict:** Declares dependency on `attach/` but does not use it. This is both a dead dependency and a functional gap (processes are rediscovered with new PIDs but never reattached to cgroups).

### 2.8 Foundation modules
- **`error/`** — standalone, used by 7+ modules. Clean.
- **`mode/`** — standalone, no deps. Clean.
- **`lexer/`** — `lexer.h` depends on `token.h` (same module). Clean.
- **`common/`** — forward declarations only. Clean.
- **`log/`** — `ilogger.h` → `error/`; `logger.h` → `ilogger.h`. Clean.

---

## 3. Cross-Module Summary

| Module | Internal deps | External deps | Depended on by |
|--------|--------------|---------------|----------------|
| `error/` | 0 | 0 | 9 modules |
| `mode/` | 0 | 0 | — |
| `common/` | 0 | 0 | — |
| `state/` | 3 (same module) | 0 | 6 modules |
| `core/` (config_domain.h) | — | 0 | 8 modules |
| `core/` (other) | — | error, lexer, driver | 3 modules |
| `lexer/` | 1 (same module) | 0 | 1 module (parser) |
| `discovery/` | 2 (same module) | state, error, core | 2 modules (monitor, recovery) |
| `driver/` | 2 (same module) | error, core | 4 modules |
| `attach/` | 1 (same module) | state, core, driver, error | 1 module (recovery) |
| `monitor/` | 2 (same module) | state, discovery, event | 0 |
| `recovery/` | 0 | state, discovery, attach, event, error | 0 |

---

## 4. Answers

### Q1: Any cyclic dependency?

**No.** The header include graph is a DAG (directed acyclic graph).

| Possible cycle | Path | Cycle? |
|---------------|------|--------|
| `monitor → discovery → state → monitor` | `monitor.h → discovery_service.h → runtime_state_manager.h`. `runtime_state_manager.h` does NOT include any `monitor/` header. | ❌ No |
| `recovery → attach → state → recovery` | `recovery_manager.h → attach_engine.h → runtime_state.h`. `runtime_state.h` does NOT include any `recovery/` header. | ❌ No |
| `core → driver → core` | `resource_transaction.h → icgroup_driver.h`. `icgroup_driver.h` includes `error.h` only — no `core/`. | ❌ No |
| `discovery → state → discovery` | `discovery_service.h → runtime_state_manager.h`. `runtime_state_manager.h` includes `runtime_state.h` only — no `discovery/`. | ❌ No |

The only potential concern is at the **directory level** rather than the header graph:
- `src/core/resource_transaction.cpp` depends on `driver/` (calls `driver_.getValue/setValue`)
- No `src/driver/` file depends back on `core/` at the implementation level (they receive `ConfigDomain`/`ConfigNode` as parameters, not by including parser logic)

### Q2: Any hidden dependency?

**Yes — one critical, one minor.**

1. **`RecoveryManager` declares `AttachEngine` dependency but never uses it (Critical).**
   - `recovery_manager.h` includes `attach_engine.h`
   - `recovery_manager.cpp` constructor stores `attachEngine_` reference
   - **No method in `recovery_manager.cpp` calls any method on `attachEngine_`**
   - At runtime, after successful process rediscovery, `AttachEngine::reattach()` is never invoked
   - Impact: recovered processes are registered with new PIDs but NOT reattached to cgroups
   - This is finding 1.3 and 3.1 from the architecture review — still unfixed

2. **`RuntimeReconciler::reconcileProcess()` modifies a copy of `RuntimeState` (Minor).**
   - `runtime_reconciler.cpp:22` calls `reconcileProcess(*stateOpt, change)` where `stateOpt` is a **copy** returned by `stateManager.findByName()`
   - All mutations on the copy (`state.updatePid()`, `state.processState().discoveryStatus = ...`) are **silently discarded**
   - The caller then applies the same changes through `stateManager.*` methods directly, so the bugs cancel out — no functional impact, but the copy modifications are wasted computation

### Q3: Any interface leakage?

**Minor leakage in two places:**

1. **`attach_engine.h` exposes `RuntimeState&` in its public API:**
   ```cpp
   std::optional<Error> attach(const ConfigNode& group, RuntimeState& state);
   ```
   The `attach` module knows about `RuntimeState` (from `state/` module) in its function signature. This couples `attach` to `state` at the interface level. Alternative: pass only `int pid` and `const std::vector<ThreadState>& threads` instead of the whole `RuntimeState`.

2. **`process_watcher.h` exposes `RuntimeStateManager&` in its public API:**
   ```cpp
   ProcessChangeSet detectDiscoveryChanges(
       const std::vector<ProcessInfo>& discovered,
       const RuntimeStateManager& stateManager) const;
   ```
   Acceptable since `RuntimeStateManager` is the module's public read interface, but it does mean `process_watcher` knows about state manager.

**No leakage** from:
- `driver/` → `core/` (driver receives `ConfigDomain`/`ConfigNode` by const ref, doesn't depend on parser internals)
- `foundation` modules (error, mode, common) — zero deps

### Q4: Any module knows too much?

**Yes — three modules have elevated knowledge breadth:**

| Module | Dependencies (distinct modules) | Assessment |
|--------|-------------------------------|-----------|
| `attach_engine` | `core`, `state`, `driver`, `error` (4) | **Appropriate** for its role as config→cgroup bridge. Each dependency is necessary: core config for group/path, state for process info, driver for cgroup ops. |
| `discovery_service` | `state` (writes to manager), `event` (publishes) | **Questionable**: writing to state manager is a dual responsibility. Consider: discovery should only detect and publish events; a coordinator should update state. |
| `recovery_manager` | `state`, `discovery`, `attach`, `event`, `error` (5) | **Excessive breadth** + the `attach` dependency is dead code. The module has the broadest dependency scope but does not actually orchestrate across all of them. |

---

## 5. Recommendations

| Priority | Issue | Action |
|----------|-------|--------|
| **High** | RecoveryManager never calls `AttachEngine::reattach()` | Add `reattach()` call after successful rediscovery in `recoverProcess()` |
| **Medium** | `RuntimeReconciler::reconcileProcess()` mutates a copy | Remove copy mutation in `reconcileProcess()` — it's dead code that creates confusion |
| **Medium** | `resource_transaction` lives in `core/` but depends on `driver/` | Move to `driver/` module for clearer layering |
| **Low** | `discovery_service` writes to `RuntimeStateManager` | Consider extracting state writes to a callback or delegating to a coordinator |
| **Low** | `attach_engine` takes full `RuntimeState&` in API | Tighten to `(int pid, const std::vector<ThreadState>& threads)` |
