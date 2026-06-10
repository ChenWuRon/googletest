set(SOURCES
    core/config_differ.cpp
    core/config_domain.cpp
    core/config_loader.cpp
    core/config_parser.cpp
    core/config_renderer.cpp
    core/config_repository.cpp
    core/parser.cpp
    core/rule_builder.cpp
    core/validator.cpp
    lexer/lexer.cpp
    mode/mode.cpp
    state/runtime_state.cpp
    state/runtime_snapshot.cpp
    state/runtime_event.cpp
    state/runtime_state_machine.cpp
    state/runtime_repository.cpp
    state/runtime_state_manager.cpp
    driver/icgroup_driver.cpp
    driver/cgroup_v2_driver.cpp
    driver/mock_cgroup_driver.cpp
    driver/control_file_type.cpp
    driver/resource_applier.cpp
    driver/resource_validator.cpp
    driver/resource_snapshot.cpp
    discovery/procfs_discovery.cpp
    discovery/discovery_rules.cpp
    discovery/discovery_cache.cpp
    discovery/discovery_service.cpp
    attach/attach_engine.cpp
    attach/attach_policy.cpp
    core/resource_transaction.cpp
    monitor/monitor.cpp
    monitor/process_watcher.cpp
    monitor/runtime_reconciler.cpp
    recovery/recovery_manager.cpp
    log/logger.cpp
    main/main.cpp
)

add_executable(resource-manager ${SOURCES})

target_include_directories(resource-manager PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
