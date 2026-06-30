cmake_minimum_required(VERSION 3.14)
project(Demo VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(demo main.cpp)
target_include_directories(demo PRIVATE ${CMAKE_SOURCE_DIR}/../third_party/include)
target_link_directories(demo PRIVATE ${CMAKE_SOURCE_DIR}/../third_party/lib)
target_link_libraries(demo gtest gtest_main pthread)

add_executable(assertions_test assertions_test.cpp)
target_include_directories(assertions_test PRIVATE ${CMAKE_SOURCE_DIR}/../third_party/include)
target_link_directories(assertions_test PRIVATE ${CMAKE_SOURCE_DIR}/../third_party/lib)
target_link_libraries(assertions_test gtest gtest_main pthread)

add_executable(fixture_test fixture_test.cpp)
target_include_directories(fixture_test PRIVATE ${CMAKE_SOURCE_DIR}/../third_party/include)
target_link_directories(fixture_test PRIVATE ${CMAKE_SOURCE_DIR}/../third_party/lib)
target_link_libraries(fixture_test gtest gtest_main pthread)

add_executable(fixture_excise fixture_excise.cpp)
target_include_directories(fixture_excise PRIVATE ${CMAKE_SOURCE_DIR}/../third_party/include)
target_link_directories(fixture_excise PRIVATE ${CMAKE_SOURCE_DIR}/../third_party/lib)
target_link_libraries(fixture_excise gtest gtest_main pthread)

add_executable(param_test param_test.cpp)
target_include_directories(param_test PRIVATE ${CMAKE_SOURCE_DIR}/../third_party/include)
target_link_directories(param_test PRIVATE ${CMAKE_SOURCE_DIR}/../third_party/lib)
target_link_libraries(param_test gtest gtest_main pthread)

add_executable(param_exercise param_excese.cpp)
target_include_directories(param_exercise PRIVATE ${CMAKE_SOURCE_DIR}/../third_party/include)
target_link_directories(param_exercise PRIVATE ${CMAKE_SOURCE_DIR}/../third_party/lib)
target_link_libraries(param_exercise gtest gtest_main pthread)

add_executable(mock_test mock_test.cpp)
target_include_directories(mock_test PRIVATE ${CMAKE_SOURCE_DIR}/../third_party/include)
target_link_directories(mock_test PRIVATE ${CMAKE_SOURCE_DIR}/../third_party/lib)
target_link_libraries(mock_test gmock gtest gtest_main pthread)

add_executable(user_service_test user_service_test.cpp)
target_include_directories(user_service_test PRIVATE ${CMAKE_SOURCE_DIR}/../third_party/include)
target_link_directories(user_service_test PRIVATE ${CMAKE_SOURCE_DIR}/../third_party/lib)
target_link_libraries(user_service_test gmock gtest gtest_main pthread)
