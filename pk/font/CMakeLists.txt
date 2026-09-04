cmake_minimum_required(VERSION 3.16)
project(pkfont CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 零 Qt：本工程不得出现 find_package(Qt...)。
add_library(pkfont STATIC
    PkFontDatabase.cpp
)
target_include_directories(pkfont PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
