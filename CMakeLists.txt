cmake_minimum_required(VERSION 3.16)
project(FileSystemTools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_BENCHMARKS "Build benchmark targets" OFF)

include(FetchContent)

find_package(CLI11 QUIET)
if(NOT CLI11_FOUND)
    FetchContent_Declare(
        cli11
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
        GIT_TAG        v2.4.1
    )
    FetchContent_MakeAvailable(cli11)
endif()

FetchContent_Declare(
    ThreadPool
    GIT_REPOSITORY https://github.com/WenXingming/ThreadPool.git
)
FetchContent_MakeAvailable(ThreadPool)

include(CTest)

add_subdirectory(src/dedup)

if(BUILD_TESTING)
    find_package(GTest QUIET)
    if(NOT GTest_FOUND)
        FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        )
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()

    add_subdirectory(test/dedup)
endif()

if(BUILD_BENCHMARKS)
    add_subdirectory(benchmark/dedup)
endif()
