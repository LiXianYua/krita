cmake_minimum_required(VERSION 3.16)
project(pktime CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 零 Qt：本工程不得出现 find_package(Qt...)。判据③ 靠 nm -u 自证。
#
# R-16 拆成 5 个 Task，本文件由 Task 1（PkElapsedTimer）新建，后续 Task 2/3
# （PkDateTime 核心值语义 + 字符串转换）会往同一份文件里追加源文件与更多测试
# ——结构照抄 pk/config/CMakeLists.txt 的骨架，让后续追加只是"加一行源文件 /
# 加一次 pk_test_generate"，不需要改整体形状。

add_library(pktime STATIC
    PkElapsedTimer.cpp
)
target_include_directories(pktime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# pk/test（R-11 交付）的 NOT TARGET 守卫用法照抄 pk/config/CMakeLists.txt：
# pk/test 自己的 CMakeLists.txt 用的是普通 project()，多个薄壳工程各自
# add_subdirectory 它时用同一个目标名 "pktest"，同一个 CMake 配置里出现两次
# 会撞名——这里没有别的 pk/* 工程会被本文件 add_subdirectory 进同一棵树，
# 但守卫依然按先例保留，防的是"本文件以后被别的壳工程一起 add_subdirectory"
# 这种尚未发生但先例已经踩过的情形。
set(PKTEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../test)
if (NOT TARGET pktest)
    add_subdirectory(${PKTEST_DIR} ${CMAKE_BINARY_DIR}/pktest)
endif()

pk_test_generate(PKTIME_BINDERS
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_elapsed_timer.h)

add_executable(test_pktime
    tests/test_main.cpp
    tests/test_elapsed_timer.cpp
)
target_include_directories(test_pktime PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${PKTEST_DIR}/compat)
target_link_libraries(test_pktime PRIVATE pktime pktest)
set_source_files_properties(tests/test_elapsed_timer.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_test_elapsed_timer.inc")
