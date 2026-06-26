add_executable(rapidcheck_test test_hash.cpp)

target_link_libraries(rapidcheck_test
    PRIVATE
    rapidcheck_lib
    GTest::gtest_main
)

# Add rapidcheck source directory to include path for the test
target_include_directories(rapidcheck_test PRIVATE ${CMAKE_SOURCE_DIR}/src/rapidcheck)

include(GoogleTest)
gtest_discover_tests(rapidcheck_test)
