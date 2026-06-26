cmake_minimum_required(VERSION 3.18)

find_package(OpenSSL REQUIRED)

# Create a library for the hasher so it can be tested
# 这样 test 就可以直接链接 rapidcheck_lib，而不需要重新编译 Sha256Hasher.cpp 等源文件，减少了编译次数和编译时间
add_library(rapidcheck_lib 
    Sha256Hash.cpp
    ProgressReporter.cpp
    FileVerifier.cpp
)
target_link_libraries(rapidcheck_lib PRIVATE OpenSSL::Crypto)

add_executable(rapidcheck main.cpp)
target_link_libraries(rapidcheck PRIVATE rapidcheck_lib CLI11::CLI11)
