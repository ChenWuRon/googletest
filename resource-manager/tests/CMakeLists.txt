find_package(GTest REQUIRED)

set(TEST_SOURCES
    core/test_config_differ.cpp
    core/test_config_domain.cpp
    core/test_config_loader.cpp
    core/test_config_parser.cpp
    core/test_config_renderer.cpp
    core/test_config_repository.cpp
    core/test_parser.cpp
    core/test_validator.cpp
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
    discovery/test_procfs_discovery.cpp
    discovery/test_discovery_rules.cpp
    discovery/test_discovery_cache.cpp
    discovery/test_discovery_service.cpp
    attach/test_attach_engine.cpp
    error/test_error.cpp
    log/test_logger.cpp
)

add_executable(resource-manager-tests ${TEST_SOURCES})

target_include_directories(resource-manager-tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(resource-manager-tests
    GTest::GTest
    GTest::Main
)

add_test(NAME resource-manager-tests COMMAND resource-manager-tests)
