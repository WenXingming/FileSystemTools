# ============================================================================
# dedup/CMakeLists - dedup CLI target
# ============================================================================

file(GLOB_RECURSE DEDUP_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)
# Exclude main.cpp from the library source list
list(REMOVE_ITEM DEDUP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")

file(GLOB_RECURSE DEDUP_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

# Core Library
add_library(dedup_lib ${DEDUP_SOURCES} ${DEDUP_HEADERS})
# Use PUBLIC so that test targets linking to this automatically get the include path
# test 直接链接 dedup_lib，而不需要重新编译大量源文件，减少了编译次数和编译时间
target_include_directories(dedup_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dedup_lib PUBLIC 
    ThreadPool
    blake3)

# Executable
add_executable(dedup main.cpp)
target_link_libraries(dedup PRIVATE 
    dedup_lib
    CLI11::CLI11)
