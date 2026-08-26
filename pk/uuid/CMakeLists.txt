cmake_minimum_required(VERSION 3.16)
project(pkuuid CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 零 Qt：本工程不得出现 find_package(Qt...)。判据③ 靠
#   nm -u <build>/libpkuuid.a | grep -i qt
# 自证。
#
# PkNodeId（QUuid 零 Qt 对应物）依赖 PkString 做解析/格式化。pk/container
# 刻意不依赖 pkstring，所以本类型独立成 pk/uuid 子目录，而不是塞进
# pk/container。S-06 遗留缺口补定义（S-05-b 交接路径）。
add_library(pkuuid STATIC
    PkNodeId.cpp
)
target_include_directories(pkuuid PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../string
    ${CMAKE_CURRENT_SOURCE_DIR}/../container
)
target_link_libraries(pkuuid PUBLIC pkstring)
