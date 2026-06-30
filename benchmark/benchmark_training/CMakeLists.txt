cmake_minimum_required(VERSION 3.10)
project(benchmark_training CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(benchmark REQUIRED)

add_executable(01_first_benchmark 01_first_benchmark.cpp)
target_link_libraries(01_first_benchmark benchmark::benchmark)

add_executable(02_custom_main 02_custom_main.cpp)
target_link_libraries(02_custom_main benchmark::benchmark)

add_executable(03_do_not_optimize 03_do_not_optimize.cpp)
target_link_libraries(03_do_not_optimize benchmark::benchmark)

add_executable(04_clobber_memory 04_clobber_memory.cpp)
target_link_libraries(04_clobber_memory benchmark::benchmark)

add_executable(05_parameterized 05_parameterized.cpp)
target_link_libraries(05_parameterized benchmark::benchmark)

add_executable(06_pause_resume 06_pause_resume.cpp)
target_link_libraries(06_pause_resume benchmark::benchmark)

add_executable(07_summary 07_summary.cpp)
target_link_libraries(07_summary benchmark::benchmark)

add_executable(08_complexity 08_complexity.cpp)
target_link_libraries(08_complexity benchmark::benchmark)

add_executable(09_fixture 09_fixture.cpp)
target_link_libraries(09_fixture benchmark::benchmark)

add_executable(10_parameter_combo 10_parameter_combo.cpp)
target_link_libraries(10_parameter_combo benchmark::benchmark)

add_executable(11_multithread 11_multithread.cpp)
target_link_libraries(11_multithread benchmark::benchmark)

add_executable(12_counters 12_counters.cpp)
target_link_libraries(12_counters benchmark::benchmark)

add_executable(13_realworld 13_realworld.cpp)
target_link_libraries(13_realworld benchmark::benchmark)
