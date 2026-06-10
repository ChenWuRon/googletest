# Recovery Architecture Resolution

**Date:** 2026-06-10
**Status:** Architecture Proposal (pre-implementation)
**Sources:** ADR-008, ADR-010, `runtime_state.h`, `attach_tracker.h`, `recovery_manager.h`, current implementations

---

## Q1: Authoritative Recovery Workflow

Per ADR-008 §Recovery Workflow and §Process Restart Lifecycle, the recovery workflow is a **3-phase pipeline** with **2 orthogonal state axes**:

### State Axes

Runtime state is tracked along three orthogonal axes:

| Axis | States | Owner |
|------|--------|-------|
| **DiscoveryStatus** | `Unknown → Discovering → Discovered → Missing → Excluded` | DiscoveryService |
| **RecoveryState** | `None → Detecting → Recovering → Recovered → Failed` | RecoveryManager |
| **AttachStatus** | `None → Pending → Attached → Detached → Failed` | AttachEngine |

These three axes evolve independently but follow a prescribed sequence:

```
Phase 1 — DETECT (Monitor)
  Discovery: Discovered ───────────────────────► Missing
  Attach:    Attached ──────────────────────────► Pending
  Recovery:  None ──────────────────────────────► Detecting

Phase 2 — REDISCOVER (RecoveryManager → DiscoveryService)
  Discovery: Missing ───────────────────────────► Discovered
  Recovery:  Detecting ─────────────────────────► Recovering
  Attach:    Pending (unchanged)

Phase 3 — REATTACH (RecoveryManager → AttachEngine)
  Attach:    Pending ───────────────────────────► Attached
  Recovery:  Recovering ───────────────────────► Recovered
  Discovery: Discovered (unchanged)

On failure at any point:
  Recovery: any ───────────────────────────────► Failed
  Attach:   Pending ───────────────────────────► Failed
```

### Complete state machine (combined view)

```
                    ┌──────────────────────────────────────────────────────────┐
                    │                    LIFECYCLE                              │
                    └──────────────────────────────────────────────────────────┘

  D:Unknown          D:Discovered       D:Discovered         D:Discovered
  R:None             R:None             R:None                R:None
  A:None ──[find]──► A:None ──[attach]──► A:Attached ──[detach]──► A:Detached
                      │                                              ▲
                      │                                              │
                      │   ┌──────────────────────────────────────┐   │
                      │   │          RECOVERY CYCLE              │   │
                      │   │                                      │   │
                      │   │  D:Missing          D:Discovered     │   │
                      │   │  R:Detecting  ────► R:Recovering     │   │
                      │   │  A:Pending          A:Pending        │   │
                      │   │       │                  │           │   │
                      │   │       │                  │           │   │
                      │   │  [retry]          ┌──────┴──────┐   │   │
                      │   │       │           │             │   │   │
                      │   │       ▼           ▼             ▼   │   │
                      │   │  D:Missing   D:Discovered   D:Discover│   │
                      │   │  R:Failed    R:Recovered    R:Failed │   │
                      │   │  A:Pending   A:Attached     A:Failed │   │
                      │   │                  ▲             │     │   │
                      │   │                  │             │     │   │
                      │   │                  └─────────────┘     │   │
                      │   └──────────────────────────────────────┘   │
                      └──────────────────────────────────────────────┘
```

---

## Q2: Data That Must Be Preserved at Attach Time

### Current state (in `RuntimeState::ProcessState`)

| Field | Available | Source | Needed for Recovery? |
|-------|-----------|--------|---------------------|
| `pid` | ✓ | Discovery | Yes — but changes after restart |
| `processName` | ✓ | Config | Yes — key to look up ConfigNode in config tree |
| `attached` (bool) | ✓ | AttachEngine | No — replace with AttachStatus enum |
| `attachTimestamp` | ✓ | AttachEngine | Nice-to-have (diagnostic) |
| `lastSeen` | ✓ | Monitor | Nice-to-have (diagnostic) |
| `discoveryStatus` | ✓ | DiscoveryService | Yes |
| `recoveryStatus` | ✓ | RecoveryManager | Yes |
| `retryCount` | ✓ | RecoveryManager | Yes |

### Missing fields

| Missing | Why Needed | Where Set | Type |
|---------|-----------|-----------|------|
| `groupPath` | `AttachEngine::reattach()` takes `const ConfigNode& group` and derives path from `group.name()`. During recovery, the ConfigNode may no longer be available (hot reload). Storing `groupPath` at attach time makes recovery self-contained. | Set by AttachEngine on successful `attach()` | `std::string` |
| `attachStatus` | Current `attached` bool is insufficient. Need `Pending` state between PID loss and reattach completion. | Set by AttachEngine / RecoveryManager | `enum class AttachStatus { None, Pending, Attached, Detached, Failed }` |

### Data NOT needed in RuntimeState

| Data | Why Not Needed | Alternative |
|------|---------------|-------------|
| `ConfigNode*` | Dangling pointer risk after hot reload (config tree replaced) | Look up by `processName` from ConfigRepository at recovery time |
| `MatchRule` | Config-specific, not process-specific | Read from ConfigRepository at recovery time via `processName` |
| Controller/item list | Same — config-derived | Read from ConfigRepository at recovery time |
| `cgroup_path` as full path | Derived from group name, but storing as string avoids config tree dependency | Store as `groupPath` |

### Recommendation: Revised `ProcessState`

```cpp
struct ProcessState {
    int pid;
    std::string processName;
    AttachStatus attachStatus;      // ← REPLACES bool attached
    std::string groupPath;          // ← NEW: cgroup path, set at attach time
    std::chrono::system_clock::time_point attachTimestamp;
    std::chrono::system_clock::time_point lastSeen;
    DiscoveryStatus discoveryStatus;
    RecoveryState recoveryStatus;
    int retryCount;
};
```

---

## Q3: Should RecoveryManager Own Access to ConfigRepository?

**YES — this is the recommended architecture.**

### Rationale

RecoveryManager needs to call `AttachEngine::reattach(const ConfigNode& group, RuntimeState& state)`. The `ConfigNode& group` is the GROUP node from the config tree. Without it, reattach cannot determine:
- The cgroup path (derived from `group.name()`)
- Which controllers to enable
- Which control files to write values to

### Ownership model

```
RecoveryManager
  ├── RuntimeStateManager&     — read/write process state
  ├── DiscoveryService&        — rediscover PID
  ├── AttachEngine&            — reattach cgroup bindings
  └── ConfigRepository&        ← NEW — look up ConfigNode GROUP
```

### Lookup method

```cpp
// RecoveryManager uses processName to find the ConfigNode GROUP
const ConfigNode* groupNode = findGroupInConfig(processName);
if (!groupNode) return Error("Group not found in config");

auto err = attachEngine_.reattach(*groupNode, state);
```

### `findGroupInConfig` implementation

Walk the config tree: `configRepo_.getRoot().root().findChild(processName)`. If the group name matches a direct child of the root node with type `GROUP`, return it.

### Alternative considered: Store full config snapshot in RuntimeState

**Rejected.** Would duplicate config data, create sync issues during hot reload, and increase memory pressure. ConfigRepository is the single source of truth for config tree structure.

---

## Q4: Should AttachTracker Store Original ConfigGroup Association?

**YES — but for observability/debugging, not as the primary recovery data path.**

### Current AttachTracker schema

```cpp
struct AttachRecord {
    int pid;
    int tid;
    std::string groupPath;        // ← already stores cgroup path
    std::chrono::system_clock::time_point timestamp;
    AttachStatus status;
};
```

### Analysis

AttachTracker currently stores `groupPath` per (pid, tid) record. This is useful for:
- Querying "what cgroup is PID 1234 attached to?"
- Listing all PIDs attached to a group path
- Diagnostics and observability

**However**, AttachTracker is **not** the right primary data source for recovery because:
1. It's per-thread, not per-process — a process with 10 threads has 10 records
2. It's a separate data structure from RuntimeState — creates a cross-module dependency during recovery
3. If attach tracker records are cleaned up or pruned, recovery loses the data

### Recommendation

1. **Primary path** (for recovery): Store `groupPath` in `RuntimeState::ProcessState` (see Q2). This is the authoritative source during recovery because it lives in the same object being recovered.

2. **Secondary path** (for diagnostics): AttachTracker already stores `groupPath`. No change needed. RecoveryManager does not need to query AttachTracker.

3. **Add** a `configGroupName` field to `AttachRecord` for cross-referencing:
   ```cpp
   struct AttachRecord {
       int pid;
       int tid;
       std::string groupPath;
       std::string configGroupName;     // ← NEW: "app", "web", etc.
       std::chrono::system_clock::time_point timestamp;
       AttachStatus status;
   };
   ```

---

## Q5: Is RuntimeState Sufficient to Perform Recovery?

**With the proposed changes: YES.**

### Current status: No

Current `RuntimeState` has `processName` but lacks `groupPath` and uses `bool attached` instead of `AttachStatus` enum. The flow breaks at the step where `AttachEngine::reattach()` needs a `ConfigNode& group`.

### With proposed changes: Yes

With `groupPath` stored in `RuntimeState` and `ConfigRepository` injected into `RecoveryManager`, the full recovery flow is:

```
RecoveryManager::recoverProcess(processName)
  │
  ├── 1. stateManager_.findByName(processName)
  │     └── returns RuntimeState with {pid, processName, groupPath, ...}
  │
  ├── 2. discovery_.findProcessByName(processName)
  │     └── returns new ProcessInfo (or nullopt)
  │
  ├── 3. [if not found] → set RecoveryState::Failed, return error
  │
  ├── 4. configRepo_.getRoot().root().findChild(processName)
  │     └── returns const ConfigNode* GROUP node
  │
  ├── 5. [if group node not found] → set RecoveryState::Failed, return error
  │
  ├── 6. stateManager_.setRecoveryStatus(Recovering)
  │
  ├── 7. attachEngine_.reattach(*groupNode, state)
  │     ├── reattach uses groupNode for controllers/items
  │     ├── reattach uses state.processState().pid for attachProcess
  │     ├── reattach uses state.processState().groupPath for cgroup path
  │     └── reattach calls state.markAttached() on success
  │
  ├── 8. [if reattach success]:
  │     ├── stateManager_.setRecoveryStatus(Recovered)
  │     └── publish RecoverySucceeded
  │
  └── 9. [if reattach failure]:
        ├── stateManager_.setRecoveryStatus(Failed)
        └── publish RecoveryFailed
```

### Remaining concern: ConfigNode pointer validity during hot reload

The `ConfigNode*` returned by `findChild()` at step 4 points into the current config tree. If a hot reload replaces the config tree between step 4 and step 7, the pointer becomes dangling.

**Mitigation**: Lock the ConfigRepository during the reattach operation (read lock). This prevents the config tree from being replaced while reattach is in progress. The lock is released after step 8/9 completes.

---

## Q6: Additional Metadata Required

### In `RuntimeState::ProcessState`

| Field | Type | Default | Set By | Used By |
|-------|------|---------|--------|---------|
| `attachStatus` | `AttachStatus` | `None` | AttachEngine | Monitor, RecoveryManager |
| `groupPath` | `std::string` | `""` | AttachEngine (on attach) | AttachEngine (on reattach) |

### In `AttachTracker::AttachRecord`

| Field | Type | Default | Set By | Used By |
|-------|------|---------|--------|---------|
| `configGroupName` | `std::string` | `""` | AttachTracker::registerAttach | Diagnostics, cross-referencing |

### New enum: `AttachStatus`

```cpp
enum class AttachStatus {
    None,
    Pending,
    Attached,
    Detached,
    Failed,
};
```

Migrate from `bool attached` to `AttachStatus attachStatus` in `ProcessState`.

### No new data structures

All recovery metadata fits within:
- `RuntimeState::ProcessState` — 2 new fields
- `AttachTracker::AttachRecord` — 1 new field
- RecoveryManager already has the required dependency slots (just needs ConfigRepository added)

---

## A. Recovery State Diagram

```
                    ┌──────────────────────────────────────────────┐
                    │         STATE TRANSITION DIAGRAM             │
                    │  Format: Discovery / Recovery / Attach       │
                    └──────────────────────────────────────────────┘


                          ┌─────────────────────┐
                          │                     │
                          │  Unknown / None /   │
                          │  None               │
                          │                     │
                          └─────────┬───────────┘
                                    │ [discover]
                                    ▼
                          ┌─────────────────────┐
                          │                     │
                          │  Discovered / None  │
                          │  / None             │
                          │                     │
                          └─────────┬───────────┘
                                    │ [attach]
                                    ▼
                    ╔══════════════════════════════╗
                    ║         STEADY STATE         ║
                    ║                              ║
                    ║  Discovered / None /         ║
              ╔═════╣  Attached                    ╠══════╗
              ║     ║                              ║      ║
              ║     ╚════════════╤═════════════════╝      ║
              ║                  │                         ║
              ║      [pid lost]  │  [detach]               ║
              ║                  ▼                         ║
              ║     ┌──────────────────────┐               ║
              ║     │                      │               ║
              ║     │  Missing / None /    │               ║
              ║     │  Pending             │               ║
              ║     │                      │               ║
              ║     └──────────┬───────────┘               ║
              ║                │                           ║
              ║     [begin recovery]                       ║
              ║                ▼                           ║
              ║     ┌──────────────────────┐               ║
              ║     │                      │               ║
              ║     │  Missing / Detecting │               ║
              ║     │  / Pending           │               ║
              ║     │                      │               ║
              ║     └──────────┬───────────┘               ║
              ║                │                           ║
              ║     [rediscover]  ── failure ──► Failed    ║
              ║                ▼                           ║
              ║     ┌──────────────────────┐               ║
              ║     │                      │               ║
              ║     │ Discovered /         │               ║
              ║     │ Recovering / Pending │               ║
              ║     │                      │               ║
              ║     └──────────┬───────────┘               ║
              ║                │                           ║
              ║     [reattach]  ── failure ──► Failed      ║
              ║                ▼                           ║
              ║     ┌──────────────────────┐               ║
              ║     │                      │               ║
              ║     │ Discovered /         │               ║
              ╚═════╣  Recovered /         ║               ║
                    │  Attached            │               ║
                    │                      │               ║
                    └──────────────────────┘               ║
                    ╚═══════════════════════════════════════╝

  Failed states (terminal for current recovery cycle):
    ┌────────────────────┐
    │                    │
    │  D:Discovered/Miss │
    │  R:Failed          │
    │  A:Failed/Pending  │
    │                    │
    └────────────────────┘
            │
    [manual retry or new detection cycle]
            ▼
    ┌────────────────────┐
    │  Back to Missing / │
    │  Detecting         │
    └────────────────────┘
```

---

## B. Recovery Sequence Diagram

### Current (broken)

```
Monitor                  RuntimeState          Discovery            AttachEngine
  │                          │                    │                     │
  ├─ poll() ─────────────────┤                    │                     │
  │                          │                    │                     │
  ├─ getAll() ──────────────►│                    │                     │
  │◄── vector<RuntimeState> ─┤                    │                     │
  │                          │                    │                     │
  ├─ findProcessByName() ────┼───────────────────►│                     │
  │◄── ProcessInfo ──────────┼────────────────────┤                     │
  │                          │                    │                     │
  ├─ reconciler.reconcile() ─┤                    │                     │
  │  ├─ updateProcessPid() ─►│                    │                     │
  │  ├─ setRecoveryStatus(   │                    │                     │
  │  │   Recovered) ────────►│  ◄── PREMATURE     │                     │
  │  └─ setDiscoveryStatus(  │                    │                     │
  │     Discovered) ────────►│                    │                     │
  │                          │                    │                     │
  │  ✗ reattach NEVER called─┼────────────────────┼────────────────────►│
  │                          │                    │                     │
  └─ publish(ProcessRestarted)                    │                     │
```

### Expected (proposed)

```
Monitor                  RecoveryManager         RuntimeState    Discovery     AttachEngine  ConfigRepo
  │                          │                       │              │              │             │
  │  poll()                  │                       │              │              │             │
  │  ├─ getAll() ────────────┼──────────────────────►│              │              │             │
  │  ├─ findProcessByName() ─┼───────────────────────┼────────────►│              │             │
  │  ├─ PID changed?         │                       │              │              │             │
  │  ├─ recoverProcess() ────►                       │              │              │             │
  │  │                       │                       │              │              │             │
  │  │  Phase 1: SET STATE   │                       │              │              │             │
  │  │  ├─ setRecoveryStatus(│                       │              │              │             │
  │  │  │  Recovering) ──────┼──────────────────────►│              │              │             │
  │  │  │                    │                       │              │              │             │
  │  │  Phase 2: REDISCOVER  │                       │              │              │             │
  │  │  ├─ findProcessByName ┼───────────────────────┼────────────►│              │             │
  │  │  │◄── ProcessInfo ────┼───────────────────────┼─────────────┤              │             │
  │  │  │                    │                       │              │              │             │
  │  │  │  [if not found:    │                       │              │              │             │
  │  │  │   set Failed,      │                       │              │              │             │
  │  │  │   return error]    │                       │              │              │             │
  │  │  │                    │                       │              │              │             │
  │  │  ├─ updateProcessPid ─┼──────────────────────►│              │              │             │
  │  │  │  (newPid)          │                       │              │              │             │
  │  │  │                    │                       │              │              │             │
  │  │  Phase 3: FIND GROUP  │                       │              │              │             │
  │  │  ├─ findChild(proc) ──┼───────────────────────┼──────────────┼──────────────┼────────────►│
  │  │  │◄── ConfigNode* ────┼───────────────────────┼──────────────┼──────────────┼─────────────┤
  │  │  │                    │                       │              │              │             │
  │  │  │  [if not found:    │                       │              │              │             │
  │  │  │   set Failed,      │                       │              │              │             │
  │  │  │   return error]    │                       │              │              │             │
  │  │  │                    │                       │              │              │             │
  │  │  Phase 4: REATTACH    │                       │              │              │             │
  │  │  ├─ reattach(group,   │                       │              │              │             │
  │  │  │   state) ──────────┼───────────────────────┼──────────────┼─────────────►│             │
  │  │  │                    │                       │              │              │             │
  │  │  │   AttachEngine::reattach():                │              │              │             │
  │  │  │   ├─ createGroup ──┼───────────────────────┼──────────────┼─────────────►│             │
  │  │  │   ├─ setValue × N ─┼───────────────────────┼──────────────┼─────────────►│             │
  │  │  │   ├─ attachProcess ┼───────────────────────┼──────────────┼─────────────►│             │
  │  │  │   ├─ attachThread  ┼───────────────────────┼──────────────┼─────────────►│             │
  │  │  │   ├─ markAttached ─┼──────────────────────►│              │              │             │
  │  │  │   └─ return nullopt│                       │              │              │             │
  │  │  │                    │                       │              │              │             │
  │  │  Phase 5: FINALIZE    │                       │              │              │             │
  │  │  ├─ setDiscoveryStatus│                       │              │              │             │
  │  │  │  (Discovered) ─────┼──────────────────────►│              │              │             │
  │  │  ├─ setRecoveryStatus │                       │              │              │             │
  │  │  │  (Recovered) ──────┼──────────────────────►│              │              │             │
  │  │  ├─ publish           │                       │              │              │             │
  │  │  │  (RecoverySucceeded)│                      │              │              │             │
  │  │  └─ return nullopt    │                       │              │              │             │
  │  │                       │                       │              │              │             │
  │  └─ poll() continues ────┼───────────────────────┤              │              │             │
```

---

## C. Required Data Model Changes

### 1. `runtime_state.h` — ProcessState

```cpp
// NEW enum
enum class AttachStatus {
    None,
    Pending,
    Attached,
    Detached,
    Failed,
};

struct ProcessState {
    int pid;
    std::string processName;
    AttachStatus attachStatus = AttachStatus::None;   // ← REPLACES bool attached
    std::string groupPath;                              // ← NEW
    std::chrono::system_clock::time_point attachTimestamp;
    std::chrono::system_clock::time_point lastSeen;
    DiscoveryStatus discoveryStatus = DiscoveryStatus::Unknown;
    RecoveryState recoveryStatus = RecoveryState::None;
    int retryCount = 0;
};
```

### 2. `runtime_state.h` — RuntimeState methods

```cpp
// REPLACE
void markAttached();    // sets attached = true
void markDetached();    // sets attached = false

// WITH
void setAttachStatus(AttachStatus status);
AttachStatus attachStatus() const;
```

### 3. `attach_tracker.h` — AttachRecord

```cpp
struct AttachRecord {
    int pid;
    int tid;
    std::string groupPath;
    std::string configGroupName;   // ← NEW
    std::chrono::system_clock::time_point timestamp;
    AttachStatus status;
};
```

### 4. `attach_tracker.h` — registerAttach signature

```cpp
// CURRENT
void registerAttach(int pid, int tid, const std::string& groupPath);

// PROPOSED (add configGroupName)
void registerAttach(int pid, int tid, const std::string& groupPath,
                    const std::string& configGroupName);
```

---

## D. Required Interface Changes

### 1. `recovery_manager.h` — Add ConfigRepository dependency

```cpp
class RecoveryManager {
public:
    RecoveryManager(
        RuntimeStateManager& stateManager,
        DiscoveryService& discovery,
        AttachEngine& attachEngine,
        ConfigRepository& configRepo     // ← NEW
    );

    // ... existing methods unchanged ...

private:
    // ... existing members ...
    ConfigRepository& configRepo_;        // ← NEW

    const ConfigNode* findGroupInConfig(const std::string& processName) const;  // ← NEW helper
};
```

### 2. `attach_engine.h` — Accept groupPath from state as optional override

```cpp
// Current: uses group.name() for cgroup path
std::optional<Error> reattach(const ConfigNode& group, RuntimeState& state);

// Proposed: use state.processState().groupPath if available,
// fall back to group.name() for backward compatibility
// (no signature change needed — read groupPath from state internally)
```

**No change to AttachEngine interface.** The `reattach()` method already receives `RuntimeState& state` — with `groupPath` added to `ProcessState`, the engine can use `state.processState().groupPath` as the cgroup path instead of (or as a fallback from) `group.name()`.

### 3. `monitor.h` — Inject RecoveryManager

```cpp
class Monitor {
public:
    Monitor(
        RuntimeStateManager& stateManager,
        DiscoveryService& discovery,
        RecoveryManager& recoveryManager,   // ← NEW
        std::chrono::milliseconds interval = std::chrono::seconds(5));

    // ... existing methods unchanged ...

private:
    // ...
    RecoveryManager& recoveryManager_;      // ← NEW
};
```

### 4. `runtime_reconciler.h` — Use AttachStatus

```cpp
// In reconcileProcess(), replace:
//   state.markAttached() / state.markDetached()
// with:
//   state.setAttachStatus(AttachStatus::Attached) / Detached
```

### 5. `runtime_state_manager.h` — Add attach status accessor

```cpp
bool setProcessAttachStatus(const std::string& name, AttachStatus status);
```

---

## E. ADR Impact Analysis

### ADR-005 Runtime State

**Impact:** Medium

| Change | ADR-005 Impact |
|--------|---------------|
| `bool attached` → `AttachStatus` enum | Field type change. ADR-005 §ProcessState must be updated to reflect the enum. All code reading `attached` must be updated. |
| New `groupPath` field | Addition. No existing code breaks. |
| New `setAttachStatus()` method | Addition. No existing code breaks. |

### ADR-008 Recovery

**Impact:** Validates and refines

| ADR-008 Section | Current | Proposed | Impact |
|-----------------|---------|----------|--------|
| §Recovery Workflow | 3-phase pipeline described but not enforced | RecoveryManager coordinates all 3 phases | ✅ Alignment — no ADR change needed |
| §Process Restart Lifecycle | Shows combined state evolution | Split into 3 orthogonal axes with AttachStatus enum | 🔶 State model refined — minor ADR update |
| §How Rediscovery Works | "Use original MatchRule" | RecoveryManager reads MatchRule from ConfigRepository | ✅ Alignment — no ADR change needed |
| §How Reattach Works | "Reattach by AttachEngine" | RecoveryManager calls AttachEngine::reattach() with ConfigNode from ConfigRepository | ✅ Alignment — no ADR change needed |
| §Recovery Ownership | "Monitor owns AttachEngine" | Monitor owns RecoveryManager, RecoveryManager owns AttachEngine | 🔶 Ownership chain adjusted — ADR §7 needs update |
| §Code Relationship | Shows Monitor calling AttachEngine directly | Monitor calls RecoveryManager, RecoveryManager calls AttachEngine | 🔶 Code examples outdated |

### ADR-010 Attach Engine

**Impact:** None

| ADR-010 Section | Current | Proposed | Impact |
|-----------------|---------|----------|--------|
| §Interface | `reattach(ConfigNode& group, RuntimeState& state)` | Unchanged — group still needed to resolve controllers/items | ✅ No change needed |
| §Reattach Flow | `reattach = attach` (same flow) | Unchanged | ✅ No change needed |
| §Retry Policy | Max 3 retries | RecoveryManager delegates retry to AttachEngine's built-in retry; RecoveryManager adds outer retry loop | ✅ Layering confirmed |
| §Why Attach Is Not Part of Monitor | Separate component | RecoveryManager sits between Monitor and AttachEngine, preserving separation | ✅ Strengthened |

### ADR-003 Config Tree

**Impact:** None

| Concern | Analysis |
|---------|----------|
| ConfigNode* validity during hot reload | Mitigated by read-locking ConfigRepository during critical section |
| Group name lookups | ConfigRepository::getRoot().root().findChild(name) — existing API, no change |
| MatchRule storage | Already in ConfigNode as mode/match children, no change needed |

### ADR-007 Process Discovery

**Impact:** Low

| Concern | Analysis |
|---------|----------|
| `DiscoveryService::findProcessByName()` | Used by RecoveryManager — works with Exact match. For wildcard/prefix groups, needs `discoverAll()` with original MatchRule. |
| MatchRule propagation | RecoveryManager reads MatchRule from ConfigRepository; passes to DiscoveryService. No DiscoveryService API change needed. |

### ADR-009 Controller Model

**Impact:** None — AttachEngine already validates via ControllerRegistry during reattach. No change needed.

---

## Summary of All Changes

| Component | Change Type | Impact |
|-----------|------------|--------|
| `RuntimeState::ProcessState` | Data model | `bool attached` → `AttachStatus` enum; add `groupPath` |
| `RuntimeState` | Interface | `markAttached/markDetached` → `setAttachStatus()` |
| `AttachTracker::AttachRecord` | Data model | Add `configGroupName` field |
| `AttachTracker::registerAttach()` | Interface | Add `configGroupName` parameter |
| `RecoveryManager` | Dependency injection | Add `ConfigRepository&` |
| `RecoveryManager::recoverProcess()` | Logic | Fix state transitions, call `attachEngine_.reattach()` |
| `Monitor` | Dependency injection | Add `RecoveryManager&` |
| `Monitor::poll()` | Logic | Delegate PID-change handling to `RecoveryManager::recoverProcess()` |
| `RuntimeStateManager` | Interface | Add `setProcessAttachStatus()` |
| `RuntimeReconciler` | Logic | Remove dead copy mutation |
| ADR-005 | Documentation | Update ProcessState field definitions |
| ADR-008 | Documentation | Update §7 Ownership chain and code examples |

### No changes needed

| Component | Reason |
|-----------|--------|
| `AttachEngine` | Interface unchanged; reads new `groupPath` from `RuntimeState` internally |
| `AttachPolicy` | No recovery-specific change needed |
| `AttachTracker` (existing API) | Backward compatible; new field is additive |
| `DiscoveryService` | No API change; RecoveryManager uses existing methods |
| `ConfigRepository` | No API change; RecoveryManager uses existing `getRoot()` |
| `ConfigNode` / `ConfigDomain` | No change |
| `ICgroupDriver` / `CgroupV2Driver` | No change |
