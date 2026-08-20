cmake_minimum_required(VERSION 3.16)
project(pkvariant CXX)

# 独立 focused 验证工程；生产 target 另由 pk/CMakeLists.txt 接入主构建。
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

enable_testing()

# 零 Qt：本工程不得出现 find_package(Qt...)。判据③ 靠
#   nm -u <build>/libpkvariant.a | grep -i qt
# 自证——静态库允许留未定义符号，真混进 Qt 依赖就会在那里现形。

add_library(pkvariant STATIC
    PkAuxTypes.cpp
    PkDataStream.cpp
    PkDataStreamColor.cpp
    PkVariant.cpp
)
target_include_directories(pkvariant PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(pkvariant PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../port)

# -fwrapv：与 pk/geometry 同一条理由——PkVariant 的 toInt/toUInt 等转换里
# 有裸的整数运算（static_cast、qRound），在 -Os 下溢出是 UB。
target_compile_options(pkvariant PUBLIC -fwrapv)

# ---------------------------------------------------------------------------
# 依赖：pk/string —— 由 geometry→test 链提供，**不直接 add_subdirectory**。
# 本工程曾直接 add_subdirectory(../string <bin>/pkstring)，而 pk/geometry
# →pk/test（pk/test/CMakeLists.txt:25）又 add_subdirectory(../string
# ${CMAKE_BINARY_DIR}/pkstring)，两者 binary dir 相同（顶层 <bin>/pkstring），
# configure 报「The binary directory ... is already used to build a source
# directory」。这是 R-14 起 pk/test 依赖 pk/string 引入的现状缺陷（R-06 之后）。
# 删掉直接 add 后，pkstring target 在 add_subdirectory(pk/geometry) 展开时已
# 由 pk/test 建好，下方 target_link_libraries(pkvariant PUBLIC pkstring ...)
# 直接引用即可。
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# 依赖：pk/geometry —— 通过 add_subdirectory 引入
# 它自带 pktest 和 pk_test_generate。
# ---------------------------------------------------------------------------
set(PKGEOMETRY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../geometry)
add_subdirectory(${PKGEOMETRY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/pkgeometry EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------
# 依赖：pk/time —— 通过 add_subdirectory 引入（Task 4 把时间类型迁到 pk/time 后，
# PkVariant 的 PkDate/PkTime/PkDateTime 实现住在 libpktime.a）。
# 必须放在 pk/geometry 之后：pk/time 的 CMakeLists 里 add_subdirectory(pk/test)
# 有 `if (NOT TARGET pktest)` 守卫，此时 pktest 已被 pk/geometry 拉进来，守卫
# 直接跳过，不会撞 binary dir。EXCLUDE_FROM_ALL 只取 pktime 库，不跟着构建
# test_pktime/date_parser_driver。
# ---------------------------------------------------------------------------
set(PKTIME_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../time)
add_subdirectory(${PKTIME_DIR} ${CMAKE_CURRENT_BINARY_DIR}/pktime EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------
# Dependency: pk/port. PkDataStream composes PkStream, so pkvariant must link
# the owning target itself. The focused shell mirrors the production wiring in
# pk/CMakeLists.txt while remaining independently configurable for tests.
# ---------------------------------------------------------------------------
set(PKPORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../port)
add_subdirectory(${PKPORT_DIR} ${CMAKE_CURRENT_BINARY_DIR}/pkport EXCLUDE_FROM_ALL)

# QColor's Qt_4_6 stream operators consume the lossless public wire state from
# pk/color. Define only its library target here instead of adding the color
# test project (which would add pk/test a second time in this focused shell).
set(PKCOLOR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../color)
set(PKGLOBAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../global)
add_library(pkglobal STATIC ${PKGLOBAL_DIR}/PkGlobal.cpp)
target_include_directories(pkglobal PUBLIC ${PKGLOBAL_DIR})
target_compile_options(pkglobal PUBLIC -fwrapv)
add_library(pkcolor STATIC ${PKCOLOR_DIR}/PkColor.cpp)
target_include_directories(pkcolor PUBLIC
    ${PKCOLOR_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../global
    ${CMAKE_CURRENT_SOURCE_DIR}/../namespace
    ${CMAKE_CURRENT_SOURCE_DIR}/../string
    ${CMAKE_CURRENT_SOURCE_DIR}/../container)
target_link_libraries(pkcolor PUBLIC pkstring pkglobal)

# ---------------------------------------------------------------------------
# 依赖：pk/container —— 仅通过 include 路径引入（纯头文件）
# pk/geometry 和 pk/container 都通过 add_subdirectory 引入 pk/test，
# 不能同时作为子目录。PkStringList.h、PkStringHash.h 是纯头文件，
# 通过 include 路径引入即可。
# ---------------------------------------------------------------------------
target_include_directories(pkvariant PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../container)
# pk/time：PkVariant.h 用 ../time/PkDateTime.h 相对 include，这里显式给上。
target_include_directories(pkvariant PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../time)

# pkvariant 链接 pkstring、pkgeometry、pktime（时间类型归位后依赖 pk/time）
target_include_directories(pkvariant PUBLIC ${PKCOLOR_DIR})
target_link_libraries(pkvariant PUBLIC pkstring pkgeometry pktime pkport pkcolor)

# ---------------------------------------------------------------------------
# 单测：用 pk/test 的 PK_* harness
# ---------------------------------------------------------------------------
pk_test_generate(PKVARIANT_GENERATED_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/variant_case.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/data_stream_case.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/color_stream_case.h")

add_executable(test_pkvariant
    tests/test_variant.cpp
)
target_link_libraries(test_pkvariant PRIVATE pkvariant pktest pkstring pktime)
target_include_directories(test_pkvariant PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../test/compat)
set_source_files_properties(tests/test_variant.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_variant_case.inc")

add_test(NAME test_pkvariant COMMAND test_pkvariant)

add_executable(test_pkdatastream
    tests/test_data_stream.cpp
)
target_link_libraries(test_pkdatastream PRIVATE pkvariant pktest pkstring pktime)
target_include_directories(test_pkdatastream PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../test/compat
    ${CMAKE_CURRENT_SOURCE_DIR}/../port)
set_source_files_properties(tests/test_data_stream.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_data_stream_case.inc")

add_test(NAME test_pkdatastream COMMAND test_pkdatastream)

add_executable(test_pkdatastream_color
    tests/test_color_stream.cpp
)
target_link_libraries(test_pkdatastream_color PRIVATE pkvariant pktest pkcolor)
target_include_directories(test_pkdatastream_color PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../test/compat)
set_source_files_properties(tests/test_color_stream.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_color_stream_case.inc")

add_test(NAME test_pkdatastream_color COMMAND test_pkdatastream_color)

# Link-only ownership guard: in Debug, PkColor's range checks reference
# pk_qt_assert. This executable intentionally links only pkcolor so the target
# must publish the assertion-runtime owner instead of borrowing it through an
# unrelated consumer such as pkgeometry.
add_executable(test_pkcolor_link tests/pkcolor_link_smoke.cpp)
target_link_libraries(test_pkcolor_link PRIVATE pkcolor)
add_test(NAME test_pkcolor_link COMMAND test_pkcolor_link)
