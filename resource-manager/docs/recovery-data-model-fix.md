# Recovery Data Model Fix — Design Proposal

**Sources:** ADR-008, ADR-010, runtime_state.h, attach_engine.h, attach_tracker.h, recovery_manager.h

---

## 1. Problem Statement

Recovery is not deterministic because:

| Gap | Impact |
|-----|--------|
| RuntimeState stores `attached=true` but not **where** the process was attached | RecoveryManager has no cgroup target path |
| AttachTracker has zero production call sites | Dead data structure — secondary metadata unavailable |
| RecoveryManager stores `attachEngine_` reference but never calls it | `recoverProcess()` sets `Recovered` without any cgroup reattachment |
| RecoveryManager has no ConfigRepository access | Cannot provide the `const ConfigNode& group` that `reattach()` requires |

**Result:** `RecoveryManager::recoverProcess()` (line 41–42) updates the PID and immediately sets `RecoveryState::Recovered`, skipping the entire reattach sequence defined in ADR-008 §Process Restart Lifecycle and ADR-010 §Reattach Flow.

---

## 2. Design Proposal

### 2.1 Add `attachedGroupPath` to RuntimeState

```cpp
struct ProcessState {
    int pid;
    std::string processName;
    bool attached;
    std::string attachedGroupPath;           // ← NEW: cgroup path from attach
    std::chrono::system_clock::time_point attachTimestamp;
    std::chrono::system_clock::time_point lastSeen;
    DiscoveryStatus discoveryStatus;
    RecoveryState recoveryStatus;
    int retryCount;
};
```

### 2.2 Persist during attach

`AttachEngine::executeAttach()` already has `cgroupPath` derived from `group.name()`. Change `state.markAttached()` to also persist the path:

```cpp
// Current (runtime_state.cpp:37-40):
void RuntimeState::markAttached() {
    processState_.attached = true;
    processState_.attachTimestamp = std::chrono::system_clock::now();
}

// Proposed:
void RuntimeState::markAttached(const std::string& groupPath) {
    processState_.attached = true;
    processState_.attachedGroupPath = groupPath;
    processState_.attachTimestamp = std::chrono::system_clock::now();
}
```

Call site in `attach_engine.cpp:91`:

```cpp
state.markAttached();           // before
state.markAttached(cgroupPath); // after
```

### 2.3 RecoveryManager shall reattach

```cpp
// RecoveryManager gains ConfigRepository&
std::optional<Error> RecoveryManager::recoverProcess(const std::string& processName) {
    // 1. Lookup state
    // 2. Rediscover process (existing)
    // 3. Update PID   (existing)
    // 4. Resolve ConfigNode from ConfigRepository via processName
    // 5. Invoke AttachEngine::reattach(group, state)  ← NEW
    // 6. Set Recovered only after success              ← FIXED ORDER
}
```

New constructor:

```cpp
RecoveryManager(
    RuntimeStateManager& stateManager,
    DiscoveryService& discovery,
    AttachEngine& attachEngine,
    ConfigRepository& configRepo);      // ← NEW
```

RecoveryManager header gains `ConfigRepository& configRepo_` member and a helper:

```cpp
std::optional<std::reference_wrapper<const ConfigNode>>
findGroupInConfig(const std::string& processName);
```

### 2.4 `RecoveryState::Recovered` only after successful reattach

Current (recovery_manager.cpp:42):

```cpp
stateManager_.setProcessRecoveryStatus(processName, RecoveryState::Recovered);
```

This must move to *after* `attachEngine_.reattach()` succeeds. The state transition becomes:

```
  setProcessDiscoveryStatus(Discovered)
       │
       ▼
  attachEngine_.reattach(group, state)   ← new
       │
       ├── success → setProcessRecoveryStatus(Recovered)
       └── failure → setProcessRecoveryStatus(Failed), return error
```

### 2.5 State machine compliance (ADR-008 §Process Restart Lifecycle)

Before (wrong):

```
Recovering (set by caller)
    → setProcessRecoveryStatus(Recovered)   ← no reattach executed
    → publishEvent(RecoverySucceeded)
```

After (correct):

```
Recovering (set by caller)
    → AttachEngine::reattach() succeeds
    → setProcessRecoveryStatus(Recovered)
    → publishEvent(RecoverySucceeded)
```

This matches the ADR-008 state machine:

```
Missing → Pending → Detecting → Discovered → Recovering → Attached → Recovered
```

---

## 3. AttachTracker Evaluation — A or B

### Option A: Make AttachTracker production state

**Changes needed:**
1. Add `AttachTracker&` to `AttachEngine` constructor
2. Call `tracker_.registerAttach(pid, tid, cgroupPath)` in `executeAttach()`
3. Call `tracker_.removeAttach()` in `detach()`
4. Potentially add `configGroupName` field to `AttachRecord`

**Pro:**
- Secondary metadata source for diagnostics
- Per-thread attach records for auditing
- queryByGroup() enables reverse lookup (which processes are in group X)

**Con:**
- Duplicates `attachedGroupPath` from `RuntimeState`
- Per-thread tracking is rarely useful (ADR-010 §4: attachProcess moves all threads)
- Adds mutable state that must be kept in sync with RuntimeState
- No consumer currently exists — would need to add query API
- Adds complexity to the hot path (registerAttach per thread)

### Option B: Remove AttachTracker entirely

**Changes needed:**
1. Delete `include/resource_manager/attach/attach_tracker.h`
2. Delete `src/attach/attach_tracker.cpp`
3. Delete `tests/attach/test_attach_tracker.cpp`
4. Remove from `CMakeLists.txt`

**Pro:**
- Eliminates dead code (zero production call sites)
- Reduces maintenance surface
- `attachedGroupPath` in RuntimeState is the single source of truth
- No sync concerns between two data structures

**Con:**
- Lose per-thread audit trail
- Lose queryByGroup() reverse lookup (can be re-added later if needed)

### Recommendation: **Option B — Remove AttachTracker entirely**

Rationale:
- ADR-010 §4 states `attachProcess` moves all threads — per-thread AttachRecords are redundant
- The `groupPath` in AttachRecord duplicates the proposed `attachedGroupPath` in RuntimeState
- No production code calls `registerAttach()` — it's pure dead weight
- If per-thread auditing is needed later, it can be added via a log-based approach
- Removing reduces the data sync problem: RuntimeState is the sole authority for attachment metadata

---

## 4. Required Code Changes

### 4.1 Header changes

| File | Change |
|------|--------|
| `include/resource_manager/state/runtime_state.h` | Add `std::string attachedGroupPath` to `ProcessState` |
| `include/resource_manager/state/runtime_state.h` | Change `markAttached()` → `markAttached(const std::string& groupPath)` |
| `include/resource_manager/state/runtime_state_manager.h` | Add `setProcessGroupPath(const std::string& name, const std::string& groupPath)` method |
| `include/resource_manager/recovery/recovery_manager.h` | Add `ConfigRepository& configRepo_` parameter and member |
| `include/resource_manager/recovery/recovery_manager.h` | Add private helper `findGroupInConfig()` |
| ~~`include/resource_manager/attach/attach_tracker.h`~~ | **Deleted** |

### 4.2 Source changes

| File | Change |
|------|--------|
| `src/state/runtime_state.cpp` | Implement `markAttached(const std::string&)`: also set `processState_.attachedGroupPath = groupPath` |
| `src/state/runtime_state_manager.cpp` | Implement `setProcessGroupPath()` |
| `src/attach/attach_engine.cpp` | `executeAttach()` line 91: `state.markAttached(cgroupPath)` instead of `state.markAttached()` |
| `src/recovery/recovery_manager.cpp` | `recoverProcess()`: add ConfigNode lookup + `attachEngine_.reattach()` call; move `setProcessRecoveryStatus(Recovered)` after success |
| `src/recovery/recovery_manager.cpp` | Add `findGroupInConfig()` private helper |
| ~~`src/attach/attach_tracker.cpp`~~ | **Deleted** |

### 4.3 Test changes

| File | Change |
|------|--------|
| `tests/attach/test_attach_tracker.cpp` | **Deleted** |
| `tests/state/test_runtime_state.cpp` | Update `markAttached()` calls to pass a groupPath string |
| `tests/state/test_runtime_state_manager.cpp` | Add tests for `setProcessGroupPath()` |
| `tests/attach/test_attach_engine.cpp` | Update to verify `attachedGroupPath` is persisted after attach |
| `tests/recovery/test_recovery_manager.cpp` | Update constructor call; add ConfigRepository mock; verify `reattach()` is called before `Recovered` |
| `tests/recovery/test_recovery_manager.cpp` | Update `recoverProcess` tests to verify `Recovered` is only set after reattach success |

### 4.4 Build changes

| File | Change |
|------|--------|
| `tests/CMakeLists.txt` | Remove `test_attach_tracker.cpp` |
| `src/CMakeLists.txt` | Remove `attach_tracker.cpp` (if listed separately) |

---

## 5. Migration Impact

### 5.1 Backward compatibility

| Concern | Impact |
|---------|--------|
| Existing RuntimeState serialization | `attachedGroupPath` added — serialization format changes; handles empty string for old records |
| AttachTracker consumers | Only tests consume AttachTracker — no migration needed for production |
| RecoveryManager construction | New `ConfigRepository&` parameter — all construction sites must be updated |
| `markAttached()` signature change | All callers must pass a groupPath — affects tests |

### 5.2 ADR compatibility

| ADR | Compatible? | Notes |
|-----|-------------|-------|
| ADR-005 Runtime State | Yes | `attachedGroupPath` extends the state model per existing pattern |
| ADR-008 Recovery | Yes | Fixes the "Recovered before reattach" violation; matches Process Restart Lifecycle state machine exactly |
| ADR-010 Attach Engine | Yes | No interface changes; only adds state persistence inside `executeAttach()` |
| ADR-013 (proposed) | This IS ADR-013 | Recovery Attachment Context — the attachment context model |

### 5.3 Risk assessment

| Risk | Level | Mitigation |
|------|-------|------------|
| `markAttached()` signature change breaks tests | Medium | Mechanical change — grep all call sites |
| RecoveryManager constructor change breaks callers | Medium | Only affects test setup code and main orchestration |
| AttachTracker deletion removes test coverage | Low | Per-thread attach tracking had zero production value; process-level tracking moves to RuntimeState |
| ConfigRepository dependency in RecoveryManager | Low | RecoveryManager already depends on discovery + attach; adding config repo is consistent |

---

## 6. Verification

After implementation, verify:

1. **RuntimeState** — `processState().attachedGroupPath` equals the group name used in `attach()`
2. **RecoveryManager** — `recoverProcess("nginx")` calls `attachEngine_.reattach(group, state)` with the correct ConfigNode resolved from ConfigRepository
3. **State transition** — `RecoveryState::Recovered` is set *only after* `reattach()` succeeds, not before
4. **Recovery failure** — when `reattach()` fails, `RecoveryState::Failed` is set and `RecoverySucceeded` is not published
5. **AttachTracker deleted** — no remaining includes, references, or build artifacts
