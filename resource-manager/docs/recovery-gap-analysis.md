# Recovery Gap Analysis

**Date:** 2026-06-10
**Sources:** ADR-008 Recovery, ADR-010 Attach Engine, current implementation

---

## 1. Intended Recovery Workflow (per ADR-008 §Recovery Workflow)

```
ADR-008 specifies three sequential phases:

  Phase 1 — DETECT
  ┌──────────────────────────────────────────────────────────────┐
  │ Monitor detects PID is gone (kill(pid,0) → ESRCH)           │
  │   → state.recovery: None → Detecting                        │
  │   → state.attach: Attached → Pending                        │
  └──────────────────────────────────────────────────────────────┘
                            │
                            ▼
  Phase 2 — REDISCOVER
  ┌──────────────────────────────────────────────────────────────┐
  │ Monitor calls Discovery.findProcesses(group.match_rule)     │
  │   → Discovery scans /proc, returns new ProcessInfo          │
  │   → state.pid = new_pid                                     │
  │   → state.discovery_status: Missing → Discovered            │
  │   → state.recovery: Detecting → Recovering                  │
  └──────────────────────────────────────────────────────────────┘
                            │
                            ▼
  Phase 3 — REATTACH (via AttachEngine)
  ┌──────────────────────────────────────────────────────────────┐
  │ Monitor calls AttachEngine::reattach(group, state)          │
  │   ├── createGroup(path)           (idempotent)              │
  │   ├── setValue(path, file, val)   × N  (rewrite all ctrls) │
  │   ├── attachProcess(path, pid)    (bind new PID)            │
  │   ├── attachThread(path, tid)     × N  (bind threads)      │
  │   └── state.markAttached()                                  │
  │   → state.recovery: Recovering → Recovered                  │
  │   → state.attach: Pending → Attached                        │
  └──────────────────────────────────────────────────────────────┘

  Ownership chain (ADR-008 §7):
    RuntimeStateManager — state owner (reads/writes by all)
    ├── reads:  Monitor (PID liveness check)
    ├── writes: Discovery (pid, discovery_status)
    └── writes: AttachEngine (attach_status)

  Monitor — flow driver
    ├── owns: IProcessDiscovery (injected)
    └── owns: AttachEngine (injected)
```

### State machine (ADR-008 §Process Restart Lifecycle)

```
Discovered ──[attach]──► Attached ──[pid lost]──► Missing ──[rediscover]──► Discovered ──[reattach]──► Attached
                              │                      │
                              │                      └──[retries exhausted]──► Failed
                              └──[detach]──► Detached
```

---

## 2. Current Implementation vs Intended Workflow

### Current `Monitor::poll()` — `monitor.cpp:42-91`

```
FOR each process state:
  call discovery_.findProcessByName(name)
  IF new PID found AND PID changed:
    → publish ProcessRestarted event
    → reconciler_.reconcile() — updates PID in stateManager
    ✗ DOES NOT call AttachEngine::reattach()
    ✗ DOES NOT call RecoveryManager
    ✗ recovery_status set to Recovered by reconciler (premature)
  ELSE IF no process found AND pid !exists:
    → publish ProcessLost event
    → reconciler_.reconcile() — marks Missing
```

### Current `RecoveryManager::recoverProcess()` — `recovery_manager.cpp:18-46`

```
  findByName(processName) in stateManager
  discovery_.findProcessByName(processName)    ← simple Exact match, no group match rule
  IF new PID found:
    → stateManager.updateProcessPid(name, newPid)
    → stateManager.setDiscoveryStatus(Discovered)
    → stateManager.setRecoveryStatus(Recovered)   ← reported BEFORE cgroup reattach
    → publish RecoverySucceeded event
    ✗ DOES NOT call attachEngine_.reattach()
    ✗ attachEngine_ reference stored but NEVER used (dead dependency)
```

### Current `AttachEngine::reattach()` — `attach_engine.cpp:35-53`

```
  Executes executeAttach(cgroupPath, group, state):
    → createGroup   (reads path from group.name())
    → setValue × N  (reads controllers/items from ConfigNode tree)
    → attachProcess (reads pid from state.processState().pid)
    → attachThread  (reads threads from state.threads())
    → state.markAttached()
  Has retry logic (max 3)
  On failure: removes cgroup + sets recovery to Failed
```

### Existing `reattach()` signature

```cpp
std::optional<Error> AttachEngine::reattach(
    const ConfigNode& group,    // needs config tree GROUP node
    RuntimeState& state);       // needs process state
```

---

## 3. Gap Analysis

### Gap 1: RecoveryManager never calls AttachEngine (Critical)
- **Evidence:** `attachEngine_` is stored at `recovery_manager.cpp:14` but no method references it
- **Impact:** After successful rediscovery, `RecoveryState` is set to `Recovered` but cgroup bindings are NOT restored. Process gets the "recovered" label while running without resource limits.
- **ADR violation:** ADR-008 §Recovery Workflow step 3 ("AttachEngine 执行 Reattach") is entirely skipped

### Gap 2: RecoveryManager reports success before cgroup reattach (Critical)
- **Evidence:** `recovery_manager.cpp:42` sets `RecoveryState::Recovered` immediately after PID update, before any cgroup operation
- **Impact:** External observers see "Recovered" status while process has no cgroup bindings. If reattach fails, the state has already been promoted to Recovered and won't be retried.
- **ADR violation:** ADR-008 §Process Restart Lifecycle shows `Recovered` only after `Attached` — reattach is a prerequisite, not a post-condition

### Gap 3: Monitor carries no ConfigNode/group context (Structural)
- **Evidence:** `Monitor::poll()` iterates `stateManager_.getAll()` which returns `RuntimeState` objects. These contain PID, processName, but NOT a reference to the config tree `ConfigNode` group.
- **Impact:** Neither `Monitor` nor `RecoveryManager` can call `AttachEngine::reattach(group, state)` because they lack the `ConfigNode& group` argument.
- **ADR violation:** ADR-008 §7 code example passes `group` directly: `attach_engine_->reattach(group, state)`

### Gap 4: RecoveryManager uses simple name match, not group match rule (Design)
- **Evidence:** `recovery_manager.cpp:29` calls `discovery_.findProcessByName(processName)` which maps to `discovery_->findProcess(MatchRule{name, "exact"})`. ADR-008 §3 specifies using the **original group match rule**.
- **Impact:** If the group uses wildcard/prefix matching, the simple exact name match may fail to rediscover the process.
- **ADR violation:** ADR-008 §Rediscovery Strategy: "使用原始 MatchRule，从 Group 中获取 match 规则，与首次发现一致"

### Gap 5: RecoveryManager has no ConfigNode access (Structural)
- **Evidence:** `RecoveryManager` constructor takes `RuntimeStateManager&`, `DiscoveryService&`, `AttachEngine&`. It has no reference to `ConfigDomain`, `ConfigRepository`, or the config tree.
- **Impact:** Cannot call `AttachEngine::reattach()` because the `ConfigNode& group` parameter is not available. The full reattach flow is structurally impossible with the current constructor signature.

### Gap 6: ReconcileProcess mutates a copy of RuntimeState (Waste)
- **Evidence:** `runtime_reconciler.cpp:12` calls `stateManager_.findByName()` which returns a **copy** of `RuntimeState`. Then `reconcileProcess(*stateOpt, change)` at line 22 mutates this copy.
- **Impact:** Copy mutations are silently lost. The caller re-applies the same changes through `stateManager_` methods, so the bug cancels out — but the copy logic is dead code and misleading.

---

## 4. Recovery Sequence Diagram

### Current Workflow (broken)

```
Monitor::poll()
  │
  ├── stateManager_.getAll()
  │     └── returns copy vector<RuntimeState>
  │
  ├── discovery_.findProcessByName(name)
  │     └── returns optional<ProcessInfo>
  │
  ├── IF pid changed:
  │     ├── reconciler_.reconcile(stateManager_, changes)
  │     │     ├── findByName() → COPY of RuntimeState
  │     │     ├── reconcileProcess(COPY) ← mutations lost
  │     │     ├── updateProcessPid()    ← real mutation
  │     │     ├── setDiscoveryStatus(Discovered)
  │     │     └── setRecoveryStatus(Recovered)  ← PREMATURE
  │     │
  │     └── publish ProcessRestarted event
  │
  │     ✗ reattach NEVER called
  │     ✗ cgroup bindings NOT restored
  │     ✗ attachEngine_ is DEAD CODE
  │
  └── return events ──→ nobody calls RecoveryManager or AttachEngine
```

### Expected Workflow (per ADR-008 + ADR-010)

```
Monitor::poll()
  │
  ├── stateManager_.getAll()
  │
  ├── discovery_.findProcessByName(name)
  │
  ├── IF pid changed:
  │     ├── stateManager_.updateProcessPid(name, newPid)
  │     ├── stateManager_.setDiscoveryStatus(Discovered)
  │     ├── stateManager_.setRecoveryStatus(Recovering)  ← NEW: intermediate state
  │     │
  │     ├── AttachEngine::reattach(group_node, state)
  │     │     ├── createGroup(path)
  │     │     ├── setValue(path, file, val) × N
  │     │     ├── attachProcess(path, newPid)
  │     │     ├── attachThread(path, tid) × N
  │     │     ├── state.markAttached()
  │     │     └── return nullopt (success) or Error
  │     │
  │     ├── IF reattach succeeded:
  │     │     ├── stateManager_.setRecoveryStatus(Recovered)
  │     │     └── publish RecoverySucceeded event
  │     │
  │     └── IF reattach failed:
  │           ├── stateManager_.setRecoveryStatus(Failed)
  │           └── publish RecoveryFailed event
  │
  └── IF process lost (not found, doesn't exist):
        ├── stateManager_.markProcessLost(name, pid)
        └── publish ProcessLost event
```

---

## 5. Answers

### Q1: What is the intended recovery workflow?

Per ADR-008 §Recovery Workflow, the recovery workflow is a 3-phase pipeline:

1. **Detect** — Monitor detects PID is gone via `kill(pid, 0) → ESRCH`
2. **Rediscover** — Monitor invokes Discovery to scan `/proc` using the original group MatchRule
3. **Reattach** — Monitor invokes AttachEngine::reattach() to restore cgroup bindings (createGroup → setValue × N → attachProcess → attachThread × N)

Each phase is owned by a separate component. Monitor is the flow driver (owns both IProcessDiscovery and AttachEngine via injection). RuntimeStateManager is the single source of truth for state.

### Q2: After rediscovery, where should AttachEngine be called?

AttachEngine should be called in **two places** depending on architecture choice:

**Option A — In Monitor::poll()** (per ADR-008 §7 code example):
- Monitor detects PID change, calls `discovery_.findProcesses(group.match)` to get new ProcessInfo, then calls `attach_engine_->reattach(group, state)` directly
- Pro: matches ADR-008 ownership diagram (Monitor owns AttachEngine)
- Con: monitor poll loop may block on cgroup I/O

**Option B — In RecoveryManager::recoverProcess()** (current design intent):
- RecoveryManager sits between Monitor and AttachEngine
- Monitor detects change → calls RecoveryManager → RecoveryManager calls DiscoveryService → RecoveryManager calls AttachEngine::reattach()
- Pro: RecoveryManager encapsulates retry/timeout/event logic
- Con: RecoveryManager currently lacks ConfigNode access

**Recommendation:** Option B (RecoveryManager as coordinator). But RecoveryManager must be given access to the config tree to obtain the `ConfigNode& group`.

### Q3: Can RecoveryManager report success before cgroup reattachment?

**No.** This is a state machine violation.

Per ADR-008 §Process Restart Lifecycle:
- `Recovered` means: PID updated AND cgroup binding restored
- The correct sequence is: `Detecting → Recovering → (reattach succeeds) → Recovered`
- Setting `RecoveryState::Recovered` before reattachment means:
  - External observers see "recovered" when the process has no resource limits
  - The state cannot transition to Failed if reattach fails (already at terminal state)
  - Recovery retry logic is bypassed

### Q4: What state transitions are currently missing?

| Transition | Current | Expected | Status |
|-----------|---------|----------|--------|
| `Detecting → Recovering` | Not implemented (skipped) | Set when rediscovery starts | ❌ Missing |
| `Recovering → Recovered` | Set prematurely after PID update | Set only after AttachEngine::reattach() succeeds | ❌ Wrong order |
| `Recovering → Failed` | Not implemented (Recovered is terminal) | Set when reattach exhausts retries | ❌ Missing |
| `Attached → Pending` | Not tracked | Set when PID is detected as lost | ❌ Missing |
| `Pending → Attached` | Not implemented | Set by AttachEngine::reattach() success | ❌ Missing |

---

## 6. Required Code Changes

### Change 1: Inject ConfigRepository into RecoveryManager

**File:** `recovery_manager.h`
- Add `ConfigRepository& configRepo_` member
- Add `std::optional<const ConfigNode*> findGroupNode(const std::string& processName)` to look up the GROUP ConfigNode from the config tree by process name

**File:** `recovery_manager.cpp`
- In `recoverProcess()`: after successful rediscovery, look up the ConfigNode group node from ConfigRepository
- Pass `group_node` to `attachEngine_.reattach(*group_node, state)`

### Change 2: Fix state transition order in recoverProcess()

**File:** `recovery_manager.cpp` — `recoverProcess()`
- Before calling attach: `stateManager_.setRecoveryStatus(Recovering)` (replace direct jump to Recovered)
- After reattach succeeds: `stateManager_.setRecoveryStatus(Recovered)`
- If reattach fails: `stateManager_.setRecoveryStatus(Failed)`, publish RecoveryFailed

### Change 3: Make Monitor carry ConfigNode context

**File:** `monitor.h` / `monitor.cpp`
- Add `ConfigRepository& configRepo_` member (inject in constructor)
- In `poll()`: after finding a PID change, look up the ConfigNode group node
- Pass the group node to RecoveryManager or directly to AttachEngine

### Change 4: Fix RuntimeReconciler dead copy mutation

**File:** `runtime_reconciler.cpp` — `reconcile()`
- Remove `reconcileProcess()` call that operates on a copy
- Drive all mutations through `stateManager_` methods directly (as the caller already does)

### Change 5: Update RecoveryManager constructor signature

**Current:**
```cpp
RecoveryManager(RuntimeStateManager&, DiscoveryService&, AttachEngine&);
```

**Required:**
```cpp
RecoveryManager(RuntimeStateManager&, DiscoveryService&, AttachEngine&, ConfigRepository&);
```

### Change 6: Remove unused code paths

- `runtime_reconciler.cpp:reconcileProcess()` — entire function operates on a copy and can be removed or converted to a pure-decision function (return action only, no side effects)

---

## Summary

| Gap | Severity | Status |
|-----|----------|--------|
| 1. RecoveryManager never calls AttachEngine | Critical | ❌ Open |
| 2. Reports success before cgroup reattach | Critical | ❌ Open |
| 3. No ConfigNode context available | Structural | ❌ Open |
| 4. Uses simple name match, not group match rule | Medium | ❌ Open |
| 5. No ConfigDomain/ConfigRepository access | Structural | ❌ Open |
| 6. reconcileProcess mutates a dead copy | Low | ❌ Open |
