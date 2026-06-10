# Integration Test Plan

**Date:** 2026-06-10
**Commit:** 140754d

---

## Scope

Validate complete end-to-end workflows across all core components: ConfigRepository → DiscoveryService → RuntimeStateManager → AttachEngine → Monitor → RecoveryManager.

All tests use mocks for kernel access (MockCgroupDriver, MockIntegrationDiscovery). No real cgroups or /proc filesystem access.

---

## Test Matrix

| ID | Scenario | Components Under Test | Priority |
|----|----------|----------------------|----------|
| IT-001 | Valid config → Parse → Validate → Success | ConfigRepository, Lexer, Parser, Validator, ConfigDomain, ConfigState | P0 |
| IT-002 | Invalid config → Parser Error | ConfigRepository, Lexer, Parser | P0 |
| IT-003 | Discovery → RuntimeState created | DiscoveryService, IProcessDiscovery, RuntimeStateManager | P0 |
| IT-004 | Attach → PID attached to target group | AttachEngine, MockCgroupDriver, RuntimeState | P0 |
| IT-005 | Attached process exit → RuntimeState Lost | Monitor, DiscoveryService, RuntimeStateManager, RuntimeReconciler | P0 |
| IT-006 | Full recovery workflow | RecoveryManager, DiscoveryService, AttachEngine, ConfigRepository, RuntimeStateManager | P0 |
| IT-007 | Driver failure → Recovery failure state | RecoveryManager, AttachEngine, MockCgroupDriver, RuntimeStateManager | P0 |

---

## Detailed Test Descriptions

### IT-001: Valid Config
- **File:** `test_integration.cpp` `IntegrationTest.IT001_ValidConfig_ParseAndValidate`
- **Input:** multi-group config with mode, match (prefix type), cpu controller (2 items), memory controller (1 item)
- **Verification points:**
  1. `loadFromString` returns true
  2. `errors()` is empty
  3. `ConfigState.source == "memory"`
  4. `ConfigState.version > 0`
  5. Tree structure: ROOT → `web-server`(GROUP) → `cpu`(CONTROLLER) → `cpu.max`(ITEM, value=`"100000 100000"`)
  6. All node types correct

### IT-002: Invalid Config
- **File:** `test_integration.cpp` `IntegrationTest.IT002_InvalidConfig_ParserError`, `IT002b`, `IT002c`
- **Inputs:** missing group name, missing closing brace, missing equals sign in item
- **Verification:** `loadFromString` returns false, `errors()` non-empty

### IT-003: Discovery
- **File:** `test_integration.cpp` `IntegrationTest.IT003_Discovery_CreatesRuntimeState`
- **Setup:** MockIntegrationDiscovery with pid=100, comm="nginx"
- **Action:** `discoverSingle(MatchRule{"nginx","exact"}, Exact)`
- **Verification:**
  1. Returns ProcessInfo with pid=100, comm="nginx"
  2. `stateManager.findByPid(100)` exists with `processName="nginx"`, `discoveryStatus=Discovered`
  3. ProcessDiscovered event published
- **No-match variant:** IT003b verifies nullopt + no state + no events

### IT-004: Attach
- **File:** `test_integration.cpp` `IntegrationTest.IT004_Attach_PidAttachedToGroup`
- **Setup:** ConfigNode tree `test_group/cpu/cpu.max=100000 100000` + `cpu.weight=100`, MockCgroupDriver, AttachEngine
- **Action:** `engine.attach(group, state)` with pid=1234
- **Verification:**
  1. No error
  2. `state.attachStatus == Attached`, `attachedGroupPath == "test_group"`
  3. Mock driver has `test_group` with `cpu.max="100000 100000"` and `cpu.weight="100"`
- **Multi-controller variant:** IT004b verifies cpu + memory with multiple items each

### IT-005: Process Exit
- **File:** `test_integration.cpp` `ProcessExitTest` fixture
- **Setup:** MockIntegrationDiscovery with pid=100 + comm="test_proc", RuntimeStateManager registered, Monitor
- **Variant 1 — No change:** `poll()` returns empty when process alive
- **Variant 2 — Lost:** remove process, `poll()` returns ProcessLost(pid=100)
- **Variant 3 — PID changed:** remove+add new PID, `poll()` returns ProcessRestarted

### IT-006: Full Recovery Workflow
- **File:** `test_integration.cpp` `RecoveryWorkflowTest.IT006_FullRecoveryWorkflow`
- **Components:** ConfigRepository (with match rule), MockIntegrationDiscovery, MockCgroupDriver, DiscoveryService, AttachEngine, RecoveryManager, RuntimeStateManager
- **Phases:**
  1. **Register:** `registerProcess("recovery_group", 100)`, set Discovered
  2. **Attach:** `engine.attach(groupNode, state)` → Attached, cgroup values set
  3. **Exit:** `markProcessLost` → Missing/Pending/Detecting
  4. **Rediscover:** remove pid=100, add pid=200 "recovery_proc"
  5. **Recover:** `recoveryManager.recoverProcess("recovery_group")` → success
- **Verification:** PID=200, RecoveryState=Recovered, attachedGroupPath set, cgroup values intact, RecoverySucceeded event

### IT-007: Driver Failure
- **File:** `test_integration.cpp` `DriverFailureTest` fixture
- **Setup:** ConfigRepository (match rule), MockIntegrationDiscovery, MockCgroupDriver, AttachEngine, RecoveryManager
- **Variant 1 — Non-retryable error:** `setNextError(ControllerNotSupported)` →
  `recoverProcess` returns ControllerNotSupported, RecoveryState=Failed, RecoveryFailed event
- **Variant 2 — Retry exhaustion:** 3 consecutive `recoverProcess` calls with injected errors →
  after 3 failures, RecoveryState=Failed

---

## Mock Strategy

| Component | Production | Test Mock |
|-----------|-----------|-----------|
| Process Discovery | ProcfsDiscovery (reads /proc) | `MockIntegrationDiscovery` — in-memory ProcessInfo vector |
| Cgroup Driver | CgroupV2Driver (writes /sys/fs/cgroup) | `MockCgroupDriver` — in-memory map, operation log, error injection |
| Discovery Orchestration | DiscoveryService | Real implementation with mock discovery backend |
| State Management | RuntimeStateManager | Real implementation (no kernel dependency) |
| Config | ConfigRepository | Real implementation with `loadFromString()` |
| Attach | AttachEngine | Real implementation with mock driver |
| Monitor | Monitor | Real implementation with mock discovery |
| Recovery | RecoveryManager | Real implementation |

---

## Pass/Fail Criteria

- All 14 integration tests must pass
- No real /proc or cgroup filesystem access
- All kernel dependencies isolated behind mock interfaces
