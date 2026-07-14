# ============================================================================
# CMakeLists.txt for dedup tests
# ============================================================================

file(GLOB_RECURSE DEDUP_TEST_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_executable(dedupTests ${DEDUP_TEST_SOURCES})

target_link_libraries(dedupTests PRIVATE
    dedup_lib
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(dedupTests)

find_program(BASH_EXECUTABLE bash REQUIRED)
add_test(
    NAME dedupCliTests
    COMMAND ${BASH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run_cli_tests.sh $<TARGET_FILE:dedup>
)
