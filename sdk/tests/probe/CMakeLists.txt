cmake_minimum_required(VERSION 3.16)
project(s00testinfraprobe CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 零 Qt：本工程不得出现 find_package(Qt...)。判据靠
#   nm -u <build>/libS00TestInfraProbe.a | grep -i qt
# 自证——静态库允许留未定义符号，真混进 Qt 依赖就会在这里现形。

# sdk/tests/probe -> sdk/tests -> sdk -> <worktree root>（pk/、libs/、sdk/
# 三个目录在这一层是兄弟目录，brief 草稿里的 "../.." 只够走到 sdk/，
# 少了一层——用 "../../.." 才落到真正的仓库根。
get_filename_component(KRITA_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

add_library(S00TestInfraProbe STATIC
    smoke.cpp
)
target_include_directories(S00TestInfraProbe PRIVATE
    # 本探针自己的试接垫片（kritaglobal_export.h / QtGlobal 的最小替身，
    # 详见 stubs/ 下各文件注释）——放在最前面，确保命中的是这两份垫片而
    # 不是别处同名文件。
    "${CMAKE_CURRENT_SOURCE_DIR}/stubs"
    "${KRITA_ROOT}/pk/container"
    "${KRITA_ROOT}/pk/geometry"
    "${KRITA_ROOT}/pk/log"
    "${KRITA_ROOT}/libs/global"
)
