find_package(GTest REQUIRED)

set(TEST_SOURCES
    core/test_config_domain.cpp
    core/test_config_parser.cpp
    core/test_parser.cpp
    core/test_validator.cpp
    lexer/test_lexer.cpp
    mode/test_mode.cpp
    state/test_runtime_state.cpp
    driver/test_cgroup_driver.cpp
    discovery/test_process_discovery.cpp
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
