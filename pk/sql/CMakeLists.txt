cmake_minimum_required(VERSION 3.16)
# CXX C：C 是给 FetchContent 兜底路径下的 sqlite3.c（amalgamation 是纯 C）用的
# ——只声明 CXX 时 CMake 找不到 sqlite3_vendored 该用哪个链接器语言，configure
# 期直接报 "CMake can not determine linker language for target:
# sqlite3_vendored"（本机没有系统 libsqlite3-dev、走 FetchContent 分支时实测
# 撞到）。find_package(SQLite3) 命中系统库那条路径不需要它，但两条路径共用
# 同一个 project() 声明，加上 C 无害。
project(pksql CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 零 Qt：本工程不得出现 find_package(Qt...)。判据③ 靠
#   nm -u -C build/libpksql.a | grep -i qt
# 自证——静态库允许留未定义符号，真混进 Qt 依赖就会在那里现形。
#
# **Task 1 的产物只有 PkSqlError.cpp/PkSqlDatabase.cpp**（PkSqlQuery 是
# Task 2 的范围），所以那一步的 nm -u 检查只覆盖了这两个类型；Task 2 新增
# PkSqlQuery.cpp/PkSqlCursor.cpp + pkvariant/pkgeometry 直接编译源码后，本
# task-2-report.md 里补一次覆盖新增产物的 nm -u 复核（仍不是终验——execBatch
# 是 Task 3 的范围，Task 4 才对完整产物做一次终版核验，R-17 plan §3 Task 1
# 最后一条）。

# sqlite3 依赖：find_package(SQLite3) → FetchContent 兜底。照抄
# docs/superpowers/plans/R-17.md §2——版本/哈希逐字来自那里，本文件不重算。
# 装了系统 libsqlite3-dev 就命中 find_package；这台机器当前没装
# （/usr/include 下没有 sqlite3.h），退化到 FetchContent 下载 amalgamation。
find_package(SQLite3 QUIET)
if (NOT SQLite3_FOUND)
    include(FetchContent)
    FetchContent_Declare(sqlite3_amalgamation
        URL https://sqlite.org/2025/sqlite-amalgamation-3500400.zip
        URL_HASH SHA256=1d3049dd0f830a025a53105fc79fd2ab9431aea99e137809d064d8ee8356b032)
    FetchContent_MakeAvailable(sqlite3_amalgamation)
    add_library(sqlite3_vendored STATIC ${sqlite3_amalgamation_SOURCE_DIR}/sqlite3.c)
    target_include_directories(sqlite3_vendored PUBLIC ${sqlite3_amalgamation_SOURCE_DIR})
    add_library(SQLite::SQLite3 ALIAS sqlite3_vendored)
endif()

# 测试用 R-11 的 PK_* harness，只是消费它，一个字节都不改。**先加它，再定义
# pksql 库**：pk/test/CMakeLists.txt 内部无条件 add_subdirectory(pk/string)
# （没有 "NOT TARGET pkstring" 守卫），所以 pk/string 这个源目录在本次
# configure 里只能被 add_subdirectory 一次——本文件不再另外单独
# add_subdirectory(pk/string)，避免同一个源目录被处理两次撞目标名。
# pkstring 目标由 pktest 带出来，pksql 库直接 target_link_libraries 即可。
#
# 与 pk/xml/CMakeLists.txt 的写法不同：那份文件在 add_subdirectory(pktest)
# 之前另外单独 guarded add_subdirectory(pkstring) 了一次。本任务执行期间
# `cmake` 命令反复撞工具链侧的间歇性不可用，没能实地复现/排除那种写法是否
# 真的不报错，所以这里选一条不依赖该假设也能正确工作的写法——供后续任务
# 核实 pk/xml 那条路径是否也需要同样处理。
set(PKTEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../test)
if (NOT TARGET pktest)
    add_subdirectory(${PKTEST_DIR} ${CMAKE_BINARY_DIR}/pktest)
endif()

# PkStringList（tables()/connectionNames() 的返回类型）是纯头文件（不进任何
# .a，理由见 pk/container/PkStringList.h 顶部注释），只需要 include 路径——
# 同 pk/variant/CMakeLists.txt 对 pk/container 的处理方式。
set(PKCONTAINER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../container)

# ---------------------------------------------------------------------------
# 依赖：pk/variant（PkVariant/PkVariantList，bindValue/value 的值类型）—— Task 2
# 新增。**不走 add_subdirectory(pk/variant)**，原因是本任务实测发现的一个
# pk/variant/CMakeLists.txt 既有 bug（不在本任务范围，未改那个文件，这里只是
# 绕开）：
#
#   pk/variant/CMakeLists.txt 自己同时 add_subdirectory(pk/string)（直接）
#   与 add_subdirectory(pk/geometry)（间接经 pk/geometry → pk/test →
#   pk/string，pk/test/CMakeLists.txt:25 写的是 ${CMAKE_BINARY_DIR}/pkstring
#   ——顶层绝对路径，不是 ${CMAKE_CURRENT_BINARY_DIR}，所以不管嵌套多深都撞
#   同一个二进制目录）——同一个源目录 pk/string 在同一次 configure 里被
#   add_subdirectory 两次，报 "is already used to build a source directory"。
#   实测复现（独立于本任务，pk/variant 单独 `cmake -S pk/variant -B <tmp>`
#   就已经炸，不需要 pk/sql 参与）：
#     CMake Error at pk/test/CMakeLists.txt:25 (add_subdirectory):
#       The binary directory .../pkstring is already used to build a
#       source directory. It cannot be used to build source directory
#       .../pk/string.
#   pk/sql 已经通过 pktest 引入了 pkstring（见上），如果再
#   add_subdirectory(pk/variant)，pk/variant 内部同样的双路径 add_subdirectory
#   会在**第一次进入 pk/variant/CMakeLists.txt 时就自炸**——不是"guard 一下
#   调用点"能绕开的（guard 只挡"被 add_subdirectory 两次"，挡不住"文件自己
#   内部两条路径互撞"）。
#
#   绕法：**不调用 pk/variant/CMakeLists.txt 与 pk/geometry/CMakeLists.txt，
#   直接把它们的源文件纳入本文件手写的 pkvariant/pkgeometry 目标**——只读
#   它们的 .cpp/.h（未改一个字节），没有再次 add_subdirectory 任何一方，
#   所以不会重放上面那个 bug。target 名字（pkgeometry/pkvariant）与 include
#   路径同它们各自 CMakeLists.txt 本来会产出的一致，下游（Task 3/4 或未来
#   任何 add_subdirectory(pk/sql) 的消费者）看到的接口形状不变。
#   这不是本任务修的 bug，是本任务绕开的 bug——修复留给 pk/variant/pk/test
#   自己的后续任务；这里只记录发现，不改那两个文件。
# ---------------------------------------------------------------------------
set(PKGEOMETRY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../geometry)
set(PKVARIANT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../variant)

if (NOT TARGET pkgeometry)
    add_library(pkgeometry STATIC
        ${PKGEOMETRY_DIR}/PkPoint.cpp
        ${PKGEOMETRY_DIR}/PkSize.cpp
        ${PKGEOMETRY_DIR}/PkRect.cpp
        ${PKGEOMETRY_DIR}/PkTransform.cpp
    )
    target_include_directories(pkgeometry PUBLIC ${PKGEOMETRY_DIR})
    # 与 pk/geometry/CMakeLists.txt 里的理由逐字相同：QPoint*qreal 等运算走
    # 裸整数运算，-fwrapv 防 UB（见该文件 target_compile_options 那段注释）。
    target_compile_options(pkgeometry PUBLIC -fwrapv)
endif()

if (NOT TARGET pkvariant)
    add_library(pkvariant STATIC
        ${PKVARIANT_DIR}/PkAuxTypes.cpp
        ${PKVARIANT_DIR}/PkVariant.cpp
    )
    target_include_directories(pkvariant PUBLIC ${PKVARIANT_DIR} ${PKCONTAINER_DIR})
    target_compile_options(pkvariant PUBLIC -fwrapv)
    target_link_libraries(pkvariant PUBLIC pkstring pkgeometry)
endif()

add_library(pksql STATIC
    PkSqlError.cpp
    PkSqlDatabase.cpp
    PkSqlQuery.cpp
    PkSqlCursor.cpp
)
target_include_directories(pksql PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${PKCONTAINER_DIR})
target_link_libraries(pksql PUBLIC pkstring pkvariant)
# PRIVATE：<sqlite3.h> 只出现在 .cpp 里，公开头文件只前置声明 `struct sqlite3;`
# （PkSqlError.h/PkSqlDatabase.h 顶部注释），不需要传递给 pksql 的使用者。
target_link_libraries(pksql PRIVATE SQLite::SQLite3)

pk_test_generate(PKSQL_BINDERS
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_error.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_database.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_query.h)

add_executable(test_pksql
    tests/test_main.cpp
    tests/test_error.cpp
    tests/test_database.cpp
    tests/test_query.cpp
)
target_include_directories(test_pksql PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${PKTEST_DIR}/compat ${CMAKE_CURRENT_SOURCE_DIR}/compat)
# 测试直接驱动裸 sqlite3 C API 构造 P1 探针的失败场景（prepare/step 失败），
# 因为 Task 1 还没有 PkSqlQuery（Task 2 的范围）——所以 test_pksql 额外链
# SQLite::SQLite3 并需要它的 include 路径，不能只靠 pksql 内部 PRIVATE 链接。
target_link_libraries(test_pksql PRIVATE pksql pktest SQLite::SQLite3)
set_source_files_properties(tests/test_error.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_test_error.inc")
set_source_files_properties(tests/test_database.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_test_database.inc")
set_source_files_properties(tests/test_query.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_test_query.inc")

enable_testing()
add_test(NAME test_pksql COMMAND test_pksql)
