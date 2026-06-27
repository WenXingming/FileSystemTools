include(FetchContent)

find_package(benchmark QUIET)
if(NOT benchmark_FOUND)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        googlebenchmark
        URL https://github.com/google/benchmark/archive/refs/tags/v1.9.5.tar.gz
    )
    FetchContent_MakeAvailable(googlebenchmark)
endif()

add_executable(rapidcheckBenchmark RapidcheckBenchmark.cpp)
target_include_directories(rapidcheckBenchmark PRIVATE ${PROJECT_SOURCE_DIR}/src/rapidcheck)
target_link_libraries(rapidcheckBenchmark PRIVATE rapidcheck_lib benchmark::benchmark)
