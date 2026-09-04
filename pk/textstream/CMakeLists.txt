cmake_minimum_required(VERSION 3.16)
project(pktextstream CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 零 Qt：本工程不得出现 find_package(Qt...)。
# PkTextStream 接 PkStream*（pk/port）做真正的设备读写；按 pk/xml 同款做法，
# 直接把 PkStream.cpp 编进本库（不 add_subdirectory(../port)，避免把 port 的
# zip/minizip 整条依赖链拖进来），值类型依赖由下方 pkstring 目标供给。
set(PKPORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../port)
set(PKSTRING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../string)
if (NOT TARGET pkstring)
    add_subdirectory(${PKSTRING_DIR} ${CMAKE_BINARY_DIR}/pkstring)
endif()

add_library(pktextstream STATIC
    PkTextStream.cpp
    ${PKPORT_DIR}/PkStream.cpp
)
target_include_directories(pktextstream PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${PKPORT_DIR} ${PKSTRING_DIR})
target_link_libraries(pktextstream PUBLIC pkstring)
