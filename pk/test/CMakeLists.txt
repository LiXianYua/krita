cmake_minimum_required(VERSION 3.16)
project(pktest CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 零 Qt：本工程不得出现 find_package(Qt...)。判据③ 靠 nm -u 自证。
add_library(pktest STATIC
    PkTestCase.cpp
    PkTestRunner.cpp
    PkTestCompare.cpp
)
target_include_directories(pktest PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(selftest_pktest
    tests/selftest_main.cpp
    tests/selftest_assert.cpp
    tests/selftest_compare.cpp
)
target_link_libraries(selftest_pktest PRIVATE pktest)
