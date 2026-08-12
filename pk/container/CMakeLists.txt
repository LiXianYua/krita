cmake_minimum_required(VERSION 3.16)
project(pkcontainer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 零 Qt：本工程不得出现 find_package(Qt...)。判据③ 靠
#   nm -u <build>/libpkcontainer.a | grep -i qt
# 自证——静态库允许留未定义符号，真混进 Qt 依赖就会在那里现形。
add_library(pkcontainer STATIC
    PkArrayData.cpp
)
target_include_directories(pkcontainer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# ---------------------------------------------------------------------------
# 单测：用 pk/test 的 PK_* harness。
#
# pk/test 是只读依赖，靠 add_subdirectory 引进来（而不是把它的源文件抄一份），
# 这样 pktest 升级时本工程自动跟上。EXCLUDE_FROM_ALL：只构建本工程真正链到的
# pktest 库，不连带把 pk/test 自己的 selftest 二进制也编一遍。
# ---------------------------------------------------------------------------
set(PKTEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../test)
add_subdirectory(${PKTEST_DIR} ${CMAKE_BINARY_DIR}/pktest EXCLUDE_FROM_ALL)

# pk_test_generate 由 pk/test 定义：为测试头生成 PkTestBinder<T> 特化 .inc。
# 产物落在**本目录**的 binary dir（函数体里用的是调用方的 CMAKE_CURRENT_BINARY_DIR）。
pk_test_generate(PKCONTAINER_GENERATED_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/PkArrayDataTest.h")

add_executable(test_pkcontainer
    tests/test_arraydata.cpp
)
target_link_libraries(test_pkcontainer PRIVATE pkcontainer pktest)

# compat/：让测试头里的 `#include <QObject>` 解析到 pk/test 的垫片
# （Q_OBJECT → friend 模板，QObject → PkTestObject）。
target_include_directories(test_pkcontainer PRIVATE ${PKTEST_DIR}/compat)

# 生成的 binder 是显式特化，必须与调用 qExec<T> 的那个 .cpp 同一个 TU
# （做法与 Qt moc 的 `#include moc_X.cpp` 惯例一致），所以只补 include 路径
# 与构建期依赖，不把它当独立源文件编译。
target_include_directories(test_pkcontainer PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
set_source_files_properties(tests/test_arraydata.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_PkArrayDataTest.inc")
