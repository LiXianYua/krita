cmake_minimum_required(VERSION 3.16)
project(pkvariant CXX)

# 独立薄壳工程，**不接入 Krita 主构建**，也不改根 CMakeLists.txt。
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
    PkVariant.cpp
)
target_include_directories(pkvariant PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

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
# 依赖：pk/container —— 仅通过 include 路径引入（纯头文件）
# pk/geometry 和 pk/container 都通过 add_subdirectory 引入 pk/test，
# 不能同时作为子目录。PkStringList.h、PkStringHash.h 是纯头文件，
# 通过 include 路径引入即可。
# ---------------------------------------------------------------------------
target_include_directories(pkvariant PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../container)

# pkvariant 链接 pkstring 和 pkgeometry
target_link_libraries(pkvariant PUBLIC pkstring pkgeometry)

# ---------------------------------------------------------------------------
# 单测：用 pk/test 的 PK_* harness
# ---------------------------------------------------------------------------
pk_test_generate(PKVARIANT_GENERATED_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/variant_case.h")

add_executable(test_pkvariant
    tests/test_variant.cpp
)
target_link_libraries(test_pkvariant PRIVATE pkvariant pktest pkstring)
target_include_directories(test_pkvariant PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../test/compat)
set_source_files_properties(tests/test_variant.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_variant_case.inc")

add_test(NAME test_pkvariant COMMAND test_pkvariant)