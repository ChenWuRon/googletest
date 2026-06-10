find_package(GTest REQUIRED)

set(LIB_SOURCES
    ${CMAKE_SOURCE_DIR}/src/core/config_differ.cpp
    ${CMAKE_SOURCE_DIR}/src/core/config_domain.cpp
    ${CMAKE_SOURCE_DIR}/src/core/config_loader.cpp
    ${CMAKE_SOURCE_DIR}/src/core/config_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/core/config_renderer.cpp
    ${CMAKE_SOURCE_DIR}/src/core/config_repository.cpp
    ${CMAKE_SOURCE_DIR}/src/core/parser.cpp
    ${CMAKE_SOURCE_DIR}/src/core/rule_builder.cpp
    ${CMAKE_SOURCE_DIR}/src/core/validator.cpp
    ${CMAKE_SOURCE_DIR}/src/core/resource_transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/driver/resource_validator.cpp
    ${CMAKE_SOURCE_DIR}/src/driver/resource_snapshot.cpp
    ${CMAKE_SOURCE_DIR}/src/lexer/lexer.cpp
    ${CMAKE_SOURCE_DIR}/src/mode/mode.cpp
    ${CMAKE_SOURCE_DIR}/src/state/runtime_state.cpp
    ${CMAKE_SOURCE_DIR}/src/state/runtime_snapshot.cpp
    ${CMAKE_SOURCE_DIR}/src/state/runtime_event.cpp
    ${CMAKE_SOURCE_DIR}/src/state/runtime_state_machine.cpp
    ${CMAKE_SOURCE_DIR}/src/state/runtime_repository.cpp
    ${CMAKE_SOURCE_DIR}/src/driver/icgroup_driver.cpp
    ${CMAKE_SOURCE_DIR}/src/driver/cgroup_v2_driver.cpp
    ${CMAKE_SOURCE_DIR}/src/driver/mock_cgroup_driver.cpp
    ${CMAKE_SOURCE_DIR}/src/driver/control_file_type.cpp
    ${CMAKE_SOURCE_DIR}/src/driver/resource_applier.cpp
    ${CMAKE_SOURCE_DIR}/src/discovery/procfs_discovery.cpp
    ${CMAKE_SOURCE_DIR}/src/discovery/discovery_rules.cpp
    ${CMAKE_SOURCE_DIR}/src/discovery/discovery_cache.cpp
    ${CMAKE_SOURCE_DIR}/src/discovery/discovery_service.cpp
    ${CMAKE_SOURCE_DIR}/src/attach/attach_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/attach/attach_policy.cpp
    ${CMAKE_SOURCE_DIR}/src/attach/attach_tracker.cpp
    ${CMAKE_SOURCE_DIR}/src/monitor/monitor.cpp
    ${CMAKE_SOURCE_DIR}/src/monitor/process_watcher.cpp
    ${CMAKE_SOURCE_DIR}/src/monitor/runtime_reconciler.cpp
    ${CMAKE_SOURCE_DIR}/src/recovery/recovery_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/log/logger.cpp
)

set(TEST_SOURCES
    core/test_config_differ.cpp
    core/test_config_domain.cpp
    core/test_config_loader.cpp
    core/test_config_parser.cpp
    core/test_config_renderer.cpp
    core/test_config_repository.cpp
    core/test_parser.cpp
    core/test_validator.cpp
    core/test_resource_transaction.cpp
    lexer/test_lexer.cpp
    mode/test_mode.cpp
    state/test_runtime_state.cpp
    state/test_runtime_snapshot.cpp
    state/test_runtime_event.cpp
    state/test_runtime_state_machine.cpp
    state/test_runtime_repository.cpp
    driver/test_cgroup_driver.cpp
    driver/test_cgroup_v2_driver.cpp
    driver/test_controller_registry.cpp
    driver/test_resource_applier.cpp
    driver/test_resource_validator.cpp
    driver/test_resource_snapshot.cpp
    discovery/test_procfs_discovery.cpp
    discovery/test_discovery_rules.cpp
    discovery/test_discovery_cache.cpp
    discovery/test_discovery_service.cpp
    attach/test_attach_engine.cpp
    attach/test_attach_policy.cpp
    attach/test_attach_tracker.cpp
    error/test_error.cpp
    log/test_logger.cpp
    monitor/test_monitor.cpp
    monitor/test_process_watcher.cpp
    monitor/test_runtime_reconciler.cpp
    recovery/test_recovery_manager.cpp
)

add_executable(resource-manager-tests ${LIB_SOURCES} ${TEST_SOURCES})

target_include_directories(resource-manager-tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(resource-manager-tests
    GTest::GTest
    GTest::Main
)

add_test(NAME resource-manager-tests COMMAND resource-manager-tests)
