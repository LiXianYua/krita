cmake_minimum_required(VERSION 3.16)
project(pkcontainer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# ctest 集成：`ninja && ctest` 是「比基线」这类完成判定的标准跑法，没有
# enable_testing() 的话 ctest 只会报 `No tests were found!!!`，只能一个一个手跑
# 13 个可执行文件。**必须在 add_test() 之前调用**，且必须在**本工程的**顶层
# CMakeLists 里调用（pk/test、pk/string 是 add_subdirectory 进来的子目录，
# 它们各自的 enable_testing() 不管本目录）。
if (NOT PKCONTAINER_LIBRARY_ONLY)
    enable_testing()
endif()

# 零 Qt：本工程不得出现 find_package(Qt...)。判据③ 靠
#   nm -u <build>/libpkcontainer.a | grep -i qt
# 自证——静态库允许留未定义符号，真混进 Qt 依赖就会在那里现形。
#
# PkStringList.h / PkPair.h 刻意**不**在这张表里：
#  - PkStringList.h 要 include pk/string 的 PkString.h，而 libpkcontainer.a
#    刻意不依赖 pkstring（理由见下面那段）。它与 PkStringHash.h 一样是纯
#    inline 头，由需要它的测试目标自己额外链 pkstring。
#  - PkPair.h 只有一个类型别名与一个函数模板，没有可实例化的类，加 .cpp 也
#    生不出任何符号。
add_library(pkcontainer STATIC
    PkByteArray.cpp
    PkArrayData.cpp
    PkVector.cpp
    PkList.cpp
    PkMap.cpp
    PkHash.cpp
    PkSet.cpp
    PkStack.cpp
    PkQueue.cpp
)
target_include_directories(pkcontainer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# ---------------------------------------------------------------------------
# 单测：用 pk/test 的 PK_* harness。
#
# pk/test 是只读依赖，靠 add_subdirectory 引进来（而不是把它的源文件抄一份），
# 这样 pktest 升级时本工程自动跟上。EXCLUDE_FROM_ALL：只构建本工程真正链到的
# pktest 库，不连带把 pk/test 自己的 selftest 二进制也编一遍。
#
# pk/test 内部已包含 pk/string 的 add_subdirectory（见 pk/test/CMakeLists.txt:
# 24-25），所以 pk/string 的库目标在这里已可用，无需再重复添加。
# ---------------------------------------------------------------------------
# Focused consumers set this normal (non-cache) variable while adding the
# canonical library target. Existing standalone consumers that also need this
# directory's test harness leave it unset and retain the full test surface.
if (NOT PKCONTAINER_LIBRARY_ONLY)
set(PKTEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../test)
add_subdirectory(${PKTEST_DIR} ${CMAKE_BINARY_DIR}/pktest EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------
# pk/string 是**只给测试用的**依赖：PkStringHash.h 在本目录里给 PkString 补了
# 一条 qHash 重载（PkHash<PkString,V> / PkSet<PkString> 是高频用法），压那条
# 链路的单测需要链到 PkString 的实现。
#
# 注意 libpkcontainer.a **本身不依赖 pkstring**：PkStringHash.h 是纯 inline，
# 库里没有一个 .cpp include 它（PkHash.cpp 的显式实例化刻意只用 int 做 key）。
# 判据③ `nm -u libpkcontainer.a` 因此不会因为这条依赖多出任何符号。
#
# pk/string 的库目标已由 pk/test 的 add_subdirectory 提供，这里只声明
# PKSTRING_DIR 供后续测试目标链接用，不再重复 add_subdirectory。
# ---------------------------------------------------------------------------
set(PKSTRING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../string)

# pk_test_generate 由 pk/test 定义：为测试头生成 PkTestBinder<T> 特化 .inc。
# 产物落在**本目录**的 binary dir（函数体里用的是调用方的 CMAKE_CURRENT_BINARY_DIR）。
pk_test_generate(PKCONTAINER_GENERATED_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/PkArrayDataTest.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/PkVectorTest.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/PkListTest.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/PkMapTest.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/PkHashTest.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/PkSetTest.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/PkStackTest.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/PkQueueTest.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/PkPairTest.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/PkStringListTest.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/PkForeachTest.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/PkJavaIteratorTest.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/PkCompatTest.h")

# 一个测试类一个可执行文件：PK_TEST_MAIN 展开出 main()，两个测试类塞进同一个
# 目标就是两个 main。与 pk/test 的既有形态一致。
#
# pk_test_add_executable(<target> <源文件> <测试类名> [额外要链的库...])
# 四件事一次做齐：建目标、链 pkcontainer+pktest（+ ARGN 里的额外库）、把
# compat/ 与生成目录挂上 include 路径（并把生成的 binder 挂成源文件的构建期
# 依赖）、注册成 ctest 用例。
function(pk_test_add_executable target source test_class)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE pkcontainer pktest ${ARGN})

    # 注册进 ctest。PK_TEST_MAIN 展开出的 main() 全绿返回 0、有失败返回非 0，
    # 正好是 ctest 的成败判据，不需要额外的 PASS_REGULAR_EXPRESSION。
    add_test(NAME ${target} COMMAND ${target})

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
pk_test_add_executable(test_pkstack     tests/test_pkstack.cpp   PkStackTest)
pk_test_add_executable(test_pkqueue     tests/test_pkqueue.cpp   PkQueueTest)
pk_test_add_executable(test_pkpair      tests/test_pkpair.cpp    PkPairTest)
# 下面三个额外链 pkstring：
#  - test_pkhash / test_pkset 压 qHash(const PkString &) 那条链路（PkStringHash.h）
#  - test_pkstringlist 的被测类型 PkStringList 本身就是 PkList<PkString>
pk_test_add_executable(test_pkhash      tests/test_pkhash.cpp    PkHashTest  pkstring)
pk_test_add_executable(test_pkset       tests/test_pkset.cpp     PkSetTest   pkstring)
pk_test_add_executable(test_pkstringlist tests/test_pkstringlist.cpp PkStringListTest pkstring)

# PK_FOREACH（Q_FOREACH/foreach）与 qDeleteAll。刻意**不**链 pkstring：
# 这两件设施与字符串无关，用 PkStack/PkQueue 覆盖派生容器那一格就够，
# 少一条依赖就少一条"其实是别人的实现在兜底"的可能。
pk_test_add_executable(test_pkforeach   tests/test_pkforeach.cpp PkForeachTest)

# Java 风格迭代器（PkListIterator / PkVectorIterator / PkMapIterator /
# PkHashIterator / PkMutableListIterator / PkMutableMapIterator）。同样不需要 pkstring。
pk_test_add_executable(test_pkjavaiterator tests/test_pkjavaiterator.cpp PkJavaIteratorTest)

# ---------------------------------------------------------------------------
# compat/ 垫片。两件事让它与其余目标不同：
#
# ① **两个源文件**：垫片是 #define，是文本替换，`<QString>` 与 `<QMap>` 的
#    include 顺序不同展开时机就不同 —— 一个 TU 压一种顺序，链进同一个可执行文件。
#    PkCompatReverseOrder.cpp 没有 main，只有 static_assert 与一个探针函数。
# ② **额外三条 include 路径**：pk/container/compat（本任务的垫片）、
#    pk/string/compat（<QString> → PkString，R-01 的交付件）、pk/string
#    （PkStringList.h / PkStringHash.h 要 include 的 PkString.h）。
#    最后那条由 pkstring 的 PUBLIC include 目录带进来，这里不重复写。
# ---------------------------------------------------------------------------
pk_test_add_executable(test_pkcompat
    "tests/test_pkcompat.cpp;tests/PkCompatReverseOrder.cpp"
    PkCompatTest pkstring)
target_include_directories(test_pkcompat PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/compat
    ${PKSTRING_DIR}/compat)
endif()
