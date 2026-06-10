# Integration Test Coverage Report

**Date:** 2026-06-10
**Commit:** 140754d
**Total Tests:** 375 (361 unit + 14 integration)

---

## Coverage by Component

| Component | Unit Tests | Integration Tests | Coverage |
|-----------|-----------|-------------------|----------|
| ConfigRepository | 4 (`test_config_repository`) | IT-001, IT-002, IT-006, IT-007 | ✅ Full |
| Lexer | 12 (`test_lexer`) | IT-001, IT-002 | ✅ Full |
| Parser | 30+ (`test_parser`) | IT-001, IT-002 | ✅ Full |
| Validator | — | IT-001 | ⚠️ Basic |
| ConfigDomain / ConfigNode | 18 (`test_config_domain`) | IT-001 | ✅ Full |
| ConfigDiffer | 16 (`test_config_differ`) | — | ✅ Unit only |
| ConfigRenderer | 9 (`test_config_renderer`) | — | ✅ Unit only |
| RuntimeState | 7 (`test_runtime_state`) | IT-003, IT-004, IT-005, IT-006 | ✅ Full |
| RuntimeStateManager | — | IT-003, IT-005, IT-006, IT-007 | ✅ Full |
| RuntimeSnapshot | 3 (`test_runtime_snapshot`) | — | ⚠️ Unit only |
| RuntimeEvent | — | IT-003, IT-006, IT-007 | ✅ Full |
| DiscoveryService | 4 (`test_discovery_service`) | IT-003, IT-005, IT-006 | ✅ Full |
| DiscoveryRules | 9 (`test_discovery_rules`) | — | ✅ Unit only |
| DiscoveryCache | — | — | ⚠️ No IT |
| ProcfsDiscovery | — | — | ⚠️ No IT (kernel access) |
| MockCgroupDriver | — | IT-004, IT-006, IT-007 | ✅ Full |
| CgroupV2Driver | 8 (`test_cgroup_v2_driver`) | — | ⚠️ No IT (kernel access) |
| CgroupV1Driver | — | — | ❌ Stub only |
| AttachEngine | 7 (`test_attach_engine`) | IT-004, IT-006, IT-007 | ✅ Full |
| AttachPolicy | — | IT-004 | ⚠️ Basic |
| Monitor | 6 (`test_monitor`) | IT-005 | ✅ Full |
| ProcessWatcher | — | — | ⚠️ No IT |
| RuntimeReconciler | — | IT-005 | ✅ Full |
| RecoveryManager | 12 (`test_recovery_manager`) | IT-006, IT-007 | ✅ Full |
| Error | — | IT-007 | ✅ Full |
| Mode | — | — | ⚠️ Unit only |
| ResourceValidator | — | — | ⚠️ Unit only |
| ControllerRegistry | — | — | ⚠️ Unit only |

---

## Integration Test Trace Matrix

| Scenario | ConfigRepo | Parser | DiscoverySvc | CgroupDriver (mock) | AttachEngine | Monitor | RuntimeStateMgr | RecoveryMgr |
|----------|-----------|--------|-------------|-------------------|-------------|---------|----------------|-------------|
| **IT-001** Valid Config | ✅ Load | ✅ Parse | — | — | — | — | — | — |
| **IT-002** Invalid Config | ✅ Load | ✅ Error | — | — | — | — | — | — |
| **IT-003** Discovery | — | — | ✅ discoverSingle | — | — | — | ✅ registerProcess | — |
| **IT-004** Attach | — | — | — | ✅ createGroup/setValue | ✅ attach | — | ✅ attachStatus | — |
| **IT-005** Exit | — | — | ✅ findProcessByName | — | — | ✅ poll | ✅ markProcessLost | — |
| **IT-006** Recovery | ✅ getRoot | — | ✅ discoverSingle | ✅ createGroup/setValue | ✅ reattach | — | ✅ updatePid/recoveryStatus | ✅ recoverProcess |
| **IT-007** Failure | ✅ getRoot | — | ✅ discoverSingle | ✅ setNextError | ✅ reattach | — | ✅ recoveryStatus | ✅ recoverProcess |

---

## Requirement Coverage

| Requirement | Test(s) | Status |
|------------|---------|--------|
| Valid config parsed and validated | IT-001 | ✅ |
| Invalid config produces errors | IT-002, IT-002b, IT-002c | ✅ |
| Discovery creates runtime state | IT-003 | ✅ |
| Non-matching discovery returns empty | IT-003b | ✅ |
| Attach creates cgroup and sets values | IT-004 | ✅ |
| Attach with multiple controllers/items | IT-004b | ✅ |
| Monitor detects no changes when healthy | ProcessExitTest.InitialPollNoEvents | ✅ |
| Monitor detects process exit | ProcessExitTest.ProcessExitTriggersLost | ✅ |
| Monitor detects PID change | ProcessExitTest.PIDChangeDetected | ✅ |
| Recovery — rediscover with new PID | IT-006 | ✅ |
| Recovery — reattach with driver | IT-006 | ✅ |
| Recovery — cgroup values preserved | IT-006 | ✅ |
| Recovery — event published | IT-006 | ✅ |
| Driver failure — recovery fails cleanly | IT-007 | ✅ |
| Driver failure — Failed state set | IT-007 | ✅ |
| Driver failure — RecoveryFailed event | IT-007 | ✅ |
| Repeated driver failures — retry exhaustion | IT-007b | ✅ |

---

## Remaining Gaps

| Gap | Priority | Notes |
|-----|----------|-------|
| **Concurrent process exit + recovery** | Medium | No test for race between Monitor detecting exit and RecoveryManager attempting recovery |
| **Multiple groups/processes simultaneously** | Medium | IT-006 tests single process; no multi-group recovery scenario |
| **Thread-level attach** | Low | IT-004 uses default process-level attach; no thread-specific test |
| **Discovery cache interaction** | Low | No integration test for DiscoveryCache + Monitor polling |
| **Monitor start/stop lifecycle with recovery** | Low | No test for stopping Monitor mid-recovery |
| **CgroupV2Driver real integration** | Low | Requires privileged container; out of scope for mock-based tests |
| **Hot Reload (ADR-011)** | High | Not implemented at all — no integration coverage possible |
| **Mode-based discovery filtering** | Medium | DiscoveryService doesn't consume Mode for filtering yet |
| **Graceful shutdown / cleanup** | Medium | No test for detach on ResourceManager shutdown |
