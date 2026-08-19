cmake_minimum_required(VERSION 3.16)
project(pkglobal)

# 独立薄壳工程，**不接入 Krita 主构建**，也不改根 CMakeLists.txt。
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 零 Qt：本工程不得出现 find_package(Qt...)。判据③ 靠 tests/run_tests.sh 里的
# nm -u libpkglobal.a 自证。
add_library(pkglobal STATIC
    PkGlobal.cpp
    PkGlobal.h
)
target_include_directories(pkglobal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# ⚠ **-fwrapv 是必需的，不是调优，而且是 PUBLIC**（消费者也要带上）。
# 与 R-03 同一条依据：qAbs(INT_MIN) 这类有符号整数溢出是 UB，真 Qt 照抄的实现
# 把这批输入当成必须钉住的形态。加 -fwrapv 后取值按二补数回绕，与 -O0 逐字相同。
# 只挡整数溢出这一类 UB；浮点→int 越界（int(inf)、int(2147483648.0)）是另一类，
# -fwrapv 管不着，靠 test_global.cpp 的 qFloor/qCeil 把边界压到运行期核对。
target_compile_options(pkglobal PUBLIC -fwrapv)

# ---------------------------------------------------------------------------
# 复用 pk/test（R-11）的 harness 与生成器 —— **只用，不改**。
# EXCLUDE_FROM_ALL：pk/test 自己的 selftest_pktest 不跟着构建，只把 pktest
# 静态库和 pk_test_generate() 函数（CMake 的 function 是全局作用域）拉进来。
# ---------------------------------------------------------------------------
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../test
                 ${CMAKE_BINARY_DIR}/pk_test
                 EXCLUDE_FROM_ALL)

pk_test_generate(PKGLOBAL_GENERATED_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/cases/global_case.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/cases/inttypes_case.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/cases/macros_case.h")

add_executable(test_pkglobal
    tests/test_main.cpp
    tests/test_global.cpp
    tests/test_inttypes.cpp
    tests/test_macros.cpp
    # 三种入口顺序下的「三份 compat/QtGlobal 共存」TU。它们能编过本身就是
    # 断言的一半，所以必须真的作为独立翻译单元编译，不能合并。
    tests/coexist_global_first.cpp
    tests/coexist_geometry_first.cpp
    tests/coexist_test_first.cpp
)
target_link_libraries(test_pkglobal PRIVATE pkglobal pktest)

# 生成的 binder 是 .inc，由各自的 test_*.cpp 自己 #include（做法与 Qt moc 的
# "#include moc_X.cpp" 惯例一致，理由见 pk/test/CMakeLists.txt 的长注释）。
# 这里只补上构建期依赖，让生成先于编译发生。
target_include_directories(test_pkglobal PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
set_source_files_properties(tests/test_global.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_global_case.inc")
set_source_files_properties(tests/test_inttypes.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_inttypes_case.inc")
set_source_files_properties(tests/test_macros.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_macros_case.inc")

# Q_UNUSED 的「-Wall -Werror 下编过」守卫：只对 test_macros.cpp 加，其余测试文件
# 不必背着这面旗。unusedDoesNotWarn 里那个「声明了却没正经用」的变量若没有被
# Q_UNUSED 消费，这里就编译失败。
set_source_files_properties(tests/test_macros.cpp PROPERTIES
    COMPILE_OPTIONS "-Wall;-Werror")
