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
    PkVector.cpp
    PkList.cpp
    PkMap.cpp
    PkHash.cpp
    PkSet.cpp
)
target_include_directories(pkcontainer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# ---------------------------------------------------------------------------
# pk/string 是**只给测试用的**依赖：PkStringHash.h 在本目录里给 PkString 补了
# 一条 qHash 重载（PkHash<PkString,V> / PkSet<PkString> 是高频用法），压那条
# 链路的单测需要链到 PkString 的实现。
#
# 注意 libpkcontainer.a **本身不依赖 pkstring**：PkStringHash.h 是纯 inline，
# 库里没有一个 .cpp include 它（PkHash.cpp 的显式实例化刻意只用 int 做 key）。
# 判据③ `nm -u libpkcontainer.a` 因此不会因为这条依赖多出任何符号。
#
# EXCLUDE_FROM_ALL：只构建真正被链到的 pkstring 库，不连带编 pk/string 自己的
# test_pkstring 二进制。
# ---------------------------------------------------------------------------
set(PKSTRING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../string)
add_subdirectory(${PKSTRING_DIR} ${CMAKE_BINARY_DIR}/pkstring EXCLUDE_FROM_ALL)

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
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/PkArrayDataTest.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/PkVectorTest.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/PkListTest.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/PkMapTest.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/PkHashTest.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/PkSetTest.h")

# 一个测试类一个可执行文件：PK_TEST_MAIN 展开出 main()，两个测试类塞进同一个
# 目标就是两个 main。与 pk/test 的既有形态一致。
#
# pk_test_add_executable(<target> <源文件> <测试类名> [额外要链的库...])
# 三件事一次做齐：建目标、链 pkcontainer+pktest（+ ARGN 里的额外库）、把
# compat/ 与生成目录挂上 include 路径，并把生成的 binder 挂成源文件的构建期依赖。
function(pk_test_add_executable target source test_class)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE pkcontainer pktest ${ARGN})

    # compat/：让测试头里的 `#include <QObject>` 解析到 pk/test 的垫片
    # （Q_OBJECT → friend 模板，QObject → PkTestObject）。
    # CMAKE_CURRENT_BINARY_DIR：生成的 binder .inc 落在那里。
    target_include_directories(${target} PRIVATE
        ${PKTEST_DIR}/compat
        ${CMAKE_CURRENT_BINARY_DIR})

    # 生成的 binder 是显式特化，必须与调用 qExec<T> 的那个 .cpp 同一个 TU
    # （做法与 Qt moc 的 `#include moc_X.cpp` 惯例一致），所以只补 include 路径
    # 与构建期依赖，不把它当独立源文件编译。
    set_source_files_properties(${source} PROPERTIES
        OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_${test_class}.inc")
endfunction()

pk_test_add_executable(test_pkcontainer tests/test_arraydata.cpp PkArrayDataTest)
pk_test_add_executable(test_pkvector    tests/test_pkvector.cpp  PkVectorTest)
pk_test_add_executable(test_pklist      tests/test_pklist.cpp    PkListTest)
pk_test_add_executable(test_pkmap       tests/test_pkmap.cpp     PkMapTest)
# 后两个额外链 pkstring：它们压 qHash(const PkString &) 那条链路（PkStringHash.h）。
pk_test_add_executable(test_pkhash      tests/test_pkhash.cpp    PkHashTest  pkstring)
pk_test_add_executable(test_pkset       tests/test_pkset.cpp     PkSetTest   pkstring)
