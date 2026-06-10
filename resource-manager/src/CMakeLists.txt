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
    driver/cgroup_v2_driver.cpp
    driver/cgroup_v1_driver.cpp
    driver/mock_cgroup_driver.cpp
    discovery/procfs_discovery.cpp
    discovery/discovery_rules.cpp
    discovery/discovery_cache.cpp
    discovery/discovery_service.cpp
    attach/attach_engine.cpp
    monitor/monitor.cpp
    log/logger.cpp
    main/main.cpp
)

add_executable(resource-manager ${SOURCES})

target_include_directories(resource-manager PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
