cmake_minimum_required(VERSION 3.18)
project(FileSystemTools LANGUAGES CXX C)

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

find_package(ThreadPool QUIET)
if(NOT ThreadPool_FOUND)
    FetchContent_Declare(
        ThreadPool
        GIT_REPOSITORY https://github.com/WenXingming/ThreadPool.git
    )
    FetchContent_MakeAvailable(ThreadPool)
endif()

find_package(blake3 QUIET)
if(NOT blake3_FOUND)
    FetchContent_Declare(
        blake3
        GIT_REPOSITORY https://github.com/BLAKE3-team/BLAKE3.git
        GIT_TAG        1.5.1
        SOURCE_SUBDIR  c
    )
    FetchContent_MakeAvailable(blake3)
endif()

include(CTest)

add_subdirectory(src)
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

    add_subdirectory(test)
endif()

if(BUILD_BENCHMARKS)
    add_subdirectory(benchmark)
endif()
