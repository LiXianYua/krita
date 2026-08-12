cmake_minimum_required(VERSION 3.16)
project(pkgeometry CXX)

# 独立薄壳工程，**不接入 Krita 主构建**，也不改根 CMakeLists.txt。
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 零 Qt：本工程不得出现 find_package(Qt...)。判据③ 靠 tests/run_tests.sh 里的
# nm -u libpkgeometry.a 自证。
add_library(pkgeometry STATIC
    PkGlobal.cpp
    PkPoint.cpp
)
target_include_directories(pkgeometry PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# ---------------------------------------------------------------------------
# 复用 pk/test（R-11）的 harness 与生成器 —— **只用，不改**。
# EXCLUDE_FROM_ALL：pk/test 自己的 selftest_pktest 不跟着构建，只把 pktest
# 静态库和 pk_test_generate() 函数（CMake 的 function 是全局作用域）拉进来。
# pk/test/CMakeLists.txt 里的 set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
# 指的是顶层构建目录，所以 libpktest.a 与 libpkgeometry.a 落在同一处。
# ---------------------------------------------------------------------------
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../test
                 ${CMAKE_CURRENT_BINARY_DIR}/pktest
                 EXCLUDE_FROM_ALL)

pk_test_generate(PKGEOMETRY_GENERATED_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/cases/global_case.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/cases/point_case.h")

add_executable(test_pkgeometry
    tests/test_main.cpp
    tests/test_global.cpp
    tests/test_point.cpp
    # 两种 include 顺序下的「两份 compat/QtGlobal 共存」TU。它们能编过本身就是
    # 断言的一半，所以必须真的作为独立翻译单元编译，不能合并。
    tests/coexist_test_first.cpp
    tests/coexist_geometry_first.cpp
    # 「PkPointF::operator== 没被 qFuzzy* 那两个 #define 改写」的探针。要证明的
    # 事情发生在预处理期，只能在一个**自己控制 include 顺序**的 TU 里测。
    tests/point_macro_proof.cpp
)
target_link_libraries(test_pkgeometry PRIVATE pkgeometry pktest)

# 生成的 binder 是 .inc，由 tests/test_global.cpp 自己 #include（做法与 Qt moc 的
# "#include moc_X.cpp" 惯例一致，理由见 pk/test/CMakeLists.txt 的长注释）。
# 这里只补上构建期依赖，让生成先于编译发生。
target_include_directories(test_pkgeometry PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
set_source_files_properties(tests/test_global.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_global_case.inc")
set_source_files_properties(tests/test_point.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_point_case.inc")
