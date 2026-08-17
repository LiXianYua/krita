cmake_minimum_required(VERSION 3.16)
project(pksignal CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 零 Qt：本工程不得出现 find_package(Qt...)。判据③ 靠 nm -u 自证。
add_library(pksignal STATIC
    PkObject.cpp
)
target_include_directories(pksignal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_pksignal
    tests/test_main.cpp
    tests/test_tree.cpp
    tests/test_connect.cpp
    tests/test_pointer.cpp
    tests/test_generator.cpp
)
target_link_libraries(test_pksignal PRIVATE pksignal)
