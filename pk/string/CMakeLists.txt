cmake_minimum_required(VERSION 3.16)
project(pkstring CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_library(pkstring STATIC
    PkStringData.cpp
    PkString_core.cpp
    PkString_query.cpp
    PkString_format.cpp
)
target_include_directories(pkstring PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_pkstring
    tests/test_main.cpp
    tests/test_core.cpp
    tests/test_query.cpp
    tests/test_format.cpp
)
target_link_libraries(test_pkstring PRIVATE pkstring)
