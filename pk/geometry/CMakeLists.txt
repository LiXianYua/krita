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
    PkSize.cpp
    PkRect.cpp
)
target_include_directories(pkgeometry PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# ⚠ **-fwrapv 是必需的，不是调优，而且是 PUBLIC**（消费者也要带上）。
# QPoint::manhattanLength 是 qAbs(x)+qAbs(y)、operator+ 是裸的 int 相加、
# dotProduct 是裸的 int 乘加 —— 在 INT_MIN/INT_MAX 上全是**有符号整数溢出 UB**，
# 而这批输入正是最该钉住的形态（Qt 自己就这么写，替代品逐字照抄）。
# 不加时优化器会拿"溢出不可能发生"去推导取值范围，实测：`-Os` 下
# pointManhattanLength() 变红，`-Os -fwrapv` 全绿。**这条是本旗标在库/单测侧的
# 全部依据**，Task 4 修复轮复核仍然复现。
# 加上之后取值按二补数回绕，与 -O0 逐字相同。
#
# ⚠ **对拍侧刻意相反：oracle/run_oracle.sh 不带 -fwrapv。** 两边不一致是有意的，
# 不是漂移 —— 对拍要与 libQt5Core.so（编好的、旗标改不动）对等，带上 -fwrapv
# 反而凭空造出 704 589 条与 PkRect 行为无关的差异。理由全文在 run_oracle.sh
# 的 CXXFLAGS_ORACLE 上方与 README「对拍侧为什么不带 -fwrapv」。
#
# 曾经写在这里的「对拍那边 -O2 无 -fwrapv 时 std::to_string 打印溢出后的负数
# 直接段错误」**已不复现**（Task 4 修复轮实测：不带 -fwrapv 跑满 125 338 365 次
# 比对，退出码 0）。那句现在时陈述已删除，别再拿它当依据。
#
# **它只挡住整数溢出这一类 UB。** 浮点→int 越界（int(inf)、int(2147483648.0)）
# 是另一类，-fwrapv 管不着，靠 tests/test_point.cpp 的 noFold() 把转换压到运行期。
# 两条都写进了 README 的覆盖度缺口。
target_compile_options(pkgeometry PUBLIC -fwrapv)

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
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/cases/point_case.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/cases/size_case.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/cases/rect_case.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/cases/rectf_case.h")

add_executable(test_pkgeometry
    tests/test_main.cpp
    tests/test_global.cpp
    tests/test_point.cpp
    tests/test_size.cpp
    tests/test_rect.cpp
    tests/test_rectf.cpp
    # 两种 include 顺序下的「两份 compat/QtGlobal 共存」TU。它们能编过本身就是
    # 断言的一半，所以必须真的作为独立翻译单元编译，不能合并。
    tests/coexist_test_first.cpp
    tests/coexist_geometry_first.cpp
    # 「PkPointF::operator== 没被 qFuzzy* 那两个 #define 改写」的探针。要证明的
    # 事情发生在预处理期，只能在一个**自己控制 include 顺序**的 TU 里测。
    tests/point_macro_proof.cpp
    # 同一件事的 Size 版：PkSizeF::operator== 的公式不能被 qFuzzy* 那两个
    # #define 在预处理期换掉。期望值与 Point 版不同（QSizeF 的 == 没有零分支）。
    tests/size_macro_proof.cpp
    # 同一件事的 RectF 版：PkRectF::operator== 的四条 qFuzzyCompare 不能被
    # qFuzzy* 那两个 #define 在预处理期换掉。期望值与 Size 版同形（QRectF 的 ==
    # 也没有 QPointF 那个零侧分支），只是多了四个分量与 ±0.0 一条。
    tests/rectf_macro_proof.cpp
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
set_source_files_properties(tests/test_size.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_size_case.inc")
set_source_files_properties(tests/test_rect.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_rect_case.inc")
set_source_files_properties(tests/test_rectf.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_rectf_case.inc")
