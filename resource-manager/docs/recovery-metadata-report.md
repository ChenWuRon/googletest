# Recovery Metadata Report

**Date:** 2026-06-10
**Scope:** Trace every field written during `AttachEngine::attach()` and assess sufficiency for recovery.

---

## 1. Full Attach Flow Trace

### Step-by-step: what is read, what is written

```
AttachEngine::attach(group, state)
  │
  ├── group.name() ─────────────────────────► "myapp" (used as cgroupPath)
  │                                           │
  │                                           └── used for createGroup / setValue / attachProcess
  │                                               NEVER written to RuntimeState
  │
  ├── executeAttach(cgroupPath, group, state)
  │     │
  │     ├── group.children() ───────────────► controller list (e.g. cpu, memory)
  │     │   └── ctrlChild.children() ───────► item list (e.g. cpu.max, memory.max)
  │     │   └── itemChild.value() ──────────► actual values (e.g. "100000", "1G")
  │     │                                      ALL read from ConfigNode at call time
  │     │                                      NEVER persisted outside config tree
  │     │
  │     ├── driver_->createGroup(cgroupPath)
  │     ├── driver_->enableController(cgroupPath, ctrlName)
  │     ├── driver_->setValue(cgroupPath, itemName, value) × N
  │     ├── driver_->attachProcess(cgroupPath, state.processState().pid)
  │     ├── driver_->attachThread(cgroupPath, tid) × N
  │     │
  │     └── state.markAttached() ───────────► writes to RuntimeState:
  │                                              attached = true
  │                                              attachTimestamp = now()
  │
  └── return nullopt
      │
      ✗ AttachTracker::registerAttach() NEVER called
      ✗ cgroupPath NEVER stored in RuntimeState
      ✗ controller/item snapshot NEVER created
```

### Who calls AttachTracker?

**No one.** `AttachEngine` has no `AttachTracker` member. `attach_engine.h` declares `driver_` and `policy_` only. The `registerAttach()` method in `attach_tracker.cpp` has **zero call sites** across the entire codebase.

---

## 2. Current Metadata — What Is Actually Persisted

### After `DiscoveryService::discoverSingle()` registers the process

| Field | Value | Stored In | Source |
|-------|-------|-----------|--------|
| `pid` | e.g. 1234 | `RuntimeState.processState.pid` | ProcfsDiscovery |
| `processName` | e.g. "nginx" | `RuntimeState.processState.processName` | ProcessInfo.comm |
| `discoveryStatus` | `Discovered` | `RuntimeState.processState.discoveryStatus` | DiscoveryService |
| `threads` | [{tid, name}, ...] | `RuntimeState.threads_` | ProcfsDiscovery::findThreads |

### After `AttachEngine::attach()` succeeds

| Field | Value | Stored In | Set By |
|-------|-------|-----------|--------|
| `attached` | `true` | `RuntimeState.processState.attached` | `state.markAttached()` |
| `attachTimestamp` | `now()` | `RuntimeState.processState.attachTimestamp` | `state.markAttached()` |

### Total metadata persisted after attach

```
RuntimeState (process "nginx", pid 1234):
  ├── pid: 1234
  ├── processName: "nginx"
  ├── attached: true
  ├── attachTimestamp: 2026-06-10T12:00:00Z
  ├── lastSeen: 2026-06-10T12:00:00Z
  ├── discoveryStatus: Discovered
  ├── recoveryStatus: None
  ├── retryCount: 0
  ├── threads: [{tid: 1234, name: "nginx"}, {tid: 1235, name: "worker"}]
  │
  └── ✗ groupPath:    (NOT STORED)
  └── ✗ controllers:  (NOT STORED)
  └── ✗ matchRule:    (NOT STORED)

AttachTracker:
  └── (empty — registerAttach() never called)
```

---

## 3. Required Metadata — What Recovery Needs

### RecoveryManager::recoverProcess() needs to call AttachEngine::reattach()

```cpp
// Required call:
auto err = attachEngine_.reattach(*groupNode, state);

// Where:
//   groupNode = const ConfigNode* — contains:
//     group.name()        → cgroup path
//     group.children()    → controller list
//     ctrl.children()     → item list + values
//
//   state = RuntimeState — needs:
//     state.processState().pid   → for attachProcess
//     state.threads()            → for attachThread
//     state.processState().groupPath  → cgroup path (currently MISSING)
```

### Breakdown of what AttachEngine::reattach() needs

| AttachEngine needs... | Currently available in RuntimeState? | Fallback |
|-----------------------|-------------------------------------|----------|
| `cgroupPath` (string) | ❌ **MISSING** — `groupPath` not stored | Could derive from `processName` (group.name()) |
| controllers + items + values | ❌ **NOT STORED** — read from ConfigNode at call time | Must read from ConfigRepository (config tree) |
| `pid` (int) | ✅ `processState.pid` | — |
| `threads` (vector) | ✅ `threads_` | — |

### Conclusion: Two metadata gaps

| Gap | Impact | Resolution |
|-----|--------|-----------|
| **No `groupPath` in RuntimeState** | RecoveryManager cannot call `reattach()` without looking up ConfigNode. Reattach is structurally impossible from RecoveryManager alone. | Store `groupPath` (derived from `group.name()`) in `RuntimeState::ProcessState` during `attach()`. |
| **No controller/item snapshot** | AttachEngine must read config tree at reattach time. Values reflect current config, not attach-time config. | **Intentional** — reattach should use current config (hot-reload changes are preserved). No snapshot needed. |

---

## 4. Can a Process Be Fully Reattached Using Only RuntimeState?

**No.** Two reasons:

### Reason 1: Missing `groupPath`

The recovery flow requires:

```
RecoveryManager → AttachEngine::reattach(group, state) → executeAttach(cgroupPath, group, state)
```

`cgroupPath` is currently derived from `group.name()` — but `group` (ConfigNode) is not available inside RecoveryManager. `RuntimeState::processName` holds the same string as `group.name()`, but nothing in the code extracts `cgroupPath` from `processName` during recovery.

Even if `processName` could serve as cgroupPath, the code never does this — the current `recoveryManager.cpp:38-42` updates PID and sets `Recovered` without any cgroup path manipulation.

### Reason 2: Missing ConfigNode context

`AttachEngine::reattach()` iterates `group.children()` to find controllers and items. Even with `groupPath` stored, the controller/item definitions come from the config tree. Without `ConfigRepository`, RecoveryManager cannot provide the `ConfigNode& group` that `reattach()` requires.

### What would make RuntimeState sufficient?

If we made RuntimeState self-sufficient for recovery, we would need to snapshot the full controller assignments into ProcessState:

```cpp
struct ControllerSnapshot {
    std::string name;
    std::vector<std::pair<std::string, std::string>> items;  // (itemName, value)
};

struct ProcessState {
    // ... existing fields ...
    std::string groupPath;                     // ← NEW
    std::vector<ControllerSnapshot> controllers; // ← NEW (optional)
};
```

**This is not recommended.** It duplicates config data, creates sync issues during hot reload, and increases memory. The correct architecture is:

> **RecoveryManager has ConfigRepository access → looks up ConfigNode by `processName` → passes to `AttachEngine::reattach()`.** Only `groupPath` needs to be added to RuntimeState.

---

## 5. Summary

| Metadata | Current | Required | Missing? |
|----------|---------|----------|----------|
| `processName` | ✅ `ProcessState.processName` | Group name for ConfigNode lookup | — |
| `pid` | ✅ `ProcessState.pid` | Process attach | — |
| `threads` | ✅ `threads_` vector | Thread attach | — |
| `attached` (bool) | ✅ `ProcessState.attached` | Was attach completed | — |
| `attachTimestamp` | ✅ `ProcessState.attachTimestamp` | Diagnostic | — |
| `groupPath` / cgroup path | ❌ **NOT STORED** | Reattach target path | **Must add** |
| Controller definitions | ❌ **NOT STORED** | Which controllers/items to write | Use ConfigRepository |
| Item values | ❌ **NOT STORED** | What values to write | Use ConfigRepository |
| MatchRule | ❌ **NOT STORED** | How to rediscover after restart | Use ConfigRepository |
| AttachTracker records | ❌ **EMPTY** (registerAttach() never called) | Potential fallback for `groupPath` | Fix caller or remove |

### Verdict

| Check | Result |
|-------|--------|
| Can reattach using only RuntimeState? | ❌ **No** |
| Minimum fix needed | 1 field: `std::string groupPath` in `ProcessState` |
| Plus architecture fix | Inject `ConfigRepository` into `RecoveryManager` |
| AttachTracker functional? | ❌ No — `registerAttach()` has zero call sites |

### One-line diagnosis

> RuntimeState stores *that* a process was attached, but not *where* it was attached (cgroup path). AttachTracker is populated by no one. Recovery has no metadata to work with.
