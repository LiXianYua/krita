# =============================================================================
# R-65 · pk 测试栈接线（fork 内的测试 compat 层 + pk_add_test 注册函数）
# =============================================================================
#
# 本文件定义在 sdk/tests/ 里。根 CMakeLists.txt 的 `add_subdirectory(sdk/tests)`
# 排在 libs / plugins / benchmarks 之前，所以这里定义的 CMake 函数与 INTERFACE
# target 对下游全部目录可见 —— 这是本任务能在锁内做成结构性迁移的唯一结构依托
# （brief §1.1）。
#
# 与 kritatestsdk 的关系：**kritatestsdk 一个字不动**（它被 58 个 CMakeLists 链，
# 其中一半在 plugins/，锁外）。这里新增一个平行的 kritatestsdk_pk。
# =============================================================================

# -----------------------------------------------------------------------------
# kritatestsdk_pk —— 与 kritatestsdk 同角色，但跑在 pk 栈上：
#   * 不链 Qt::Test / Qt::Widgets（于是 Qt 头路径与 -DQT_* 宏随之中止，见 brief §2.1
#     配方 1：Qt 头路径来自 Qt5::Test/Qt5::Widgets 的 INTERFACE include，不链就没了）；
#   * INTERFACE include 目录 = 每个 pk/<模块>/compat + 本目录 compat；
#   * 以 -include 强制注入 PkTestCompatAll.h 预激活 compat 宏（配方 3）；
#   * 链 pktest（测试运行时）。
# -----------------------------------------------------------------------------
add_library(kritatestsdk_pk INTERFACE)

# sdk/tests 必须排在 pk/test/compat 之前：`#include <simpletest.h>` 要解析到
# sdk/tests/simpletest.h 的真品（PkThread 登记 + runSimpleTest），不是
# pk/test/compat/simpletest.h 的最小转发。本目录的 source/binary 目录由 INTERFACE
# 库的自动 include 也带上，这里**显式再列一次并放在最前**，保证顺序。
target_include_directories(kritatestsdk_pk INTERFACE
    # 1) sdk/tests 必须最先：`#include <simpletest.h>`/`<kistest.h>` 要解析到本目录
    #    的真品（PkThread 登记 + runSimpleTest），不是 pk/test/compat 下的最小转发。
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}"
    # 2) fork 内的本地垫片目录：pk/*/compat 里没有、而测试面真实需要的名字。
    #    排在 pk/*/compat 之前 —— 同名时本地垫片优先（顺序同薄壳 SHELL_COMPAT_DIRS）。
    "${CMAKE_CURRENT_SOURCE_DIR}/compat"
    # 3) 仓库根：wrap TU 写的是 <pk/test/compat/QObject>（工程根相对）——
    #    薄壳 SHELL_INCLUDE_DIRS 收 KRITA_ROOT 同因。
    "${CMAKE_SOURCE_DIR}"
    # 4) pk 各模块**源码**目录：pk/*/compat 下的垫片用裸名 include 本模块头
    #    （例：pk/variant/compat/QVariant 里是 `#include "PkVariant.h"`，不是
    #    `../PkVariant.h`），所以模块目录本身必须在搜索路径上。清单与薄壳
    #    SHELL_INCLUDE_DIRS 的 pk 段一一对应。
    "${CMAKE_SOURCE_DIR}/pk/namespace"
    "${CMAKE_SOURCE_DIR}/pk/string"
    "${CMAKE_SOURCE_DIR}/pk/container"
    "${CMAKE_SOURCE_DIR}/pk/variant"
    "${CMAKE_SOURCE_DIR}/pk/uuid"
    "${CMAKE_SOURCE_DIR}/pk/port"
    "${CMAKE_SOURCE_DIR}/pk/pointer"
    "${CMAKE_SOURCE_DIR}/pk/geometry"
    "${CMAKE_SOURCE_DIR}/pk/global"
    "${CMAKE_SOURCE_DIR}/pk/log"
    "${CMAKE_SOURCE_DIR}/pk/concurrent"
    "${CMAKE_SOURCE_DIR}/pk/image"
    "${CMAKE_SOURCE_DIR}/pk/time"
    "${CMAKE_SOURCE_DIR}/pk/config"
    "${CMAKE_SOURCE_DIR}/pk/flags"
    "${CMAKE_SOURCE_DIR}/pk/color"
    "${CMAKE_SOURCE_DIR}/pk/signal"
    "${CMAKE_SOURCE_DIR}/pk/sql"
    "${CMAKE_SOURCE_DIR}/pk/xml"
    # 5) pk 各模块 compat 垫片（裸 <QString>/<QObject>/… 由这里解析）。
    "${CMAKE_SOURCE_DIR}/pk/global/compat"
    "${CMAKE_SOURCE_DIR}/pk/test/compat"
    "${CMAKE_SOURCE_DIR}/pk/string/compat"
    "${CMAKE_SOURCE_DIR}/pk/container/compat"
    "${CMAKE_SOURCE_DIR}/pk/color/compat"
    "${CMAKE_SOURCE_DIR}/pk/geometry/compat"
    "${CMAKE_SOURCE_DIR}/pk/image/compat"
    "${CMAKE_SOURCE_DIR}/pk/log/compat"
    "${CMAKE_SOURCE_DIR}/pk/signal/compat"
    "${CMAKE_SOURCE_DIR}/pk/variant/compat"
    "${CMAKE_SOURCE_DIR}/pk/xml/compat"
    "${CMAKE_SOURCE_DIR}/pk/flags/compat"
    "${CMAKE_SOURCE_DIR}/pk/pointer/compat"
    "${CMAKE_SOURCE_DIR}/pk/concurrent/compat"
    "${CMAKE_SOURCE_DIR}/pk/time/compat"
    "${CMAKE_SOURCE_DIR}/pk/port/compat"
    "${CMAKE_SOURCE_DIR}/pk/config/compat"
    "${CMAKE_SOURCE_DIR}/pk/sql/compat"
)

# 预激活聚合头：对每个 TU 强制 -include。用绝对路径（工作目录不固定）。
target_compile_options(kritatestsdk_pk INTERFACE
    "-include" "${CMAKE_CURRENT_SOURCE_DIR}/PkTestCompatAll.h")

# pkconcurrent：sdk/tests/simpletest.h 的 SIMPLE_MAIN_IMPL 在 KRITA_TESTSDK_PK_NATIVE
# 下直接调 PkThread::registerMainThread() / PkThreadCallQueue::warmUpCurrentThread()
# ——任何走 SIMPLE_TEST_MAIN 族的目标都需要这两个符号，所以它是测试 SDK 的一部分
# （与 kritatestsdk 把 kritaglobal 一起链走同因），不该要求下游各自记得加。
target_link_libraries(kritatestsdk_pk INTERFACE pktest pkconcurrent)

# -----------------------------------------------------------------------------
# pk_add_test(<testbase> [SRCDIR <dir>] [LINK_LIBRARIES <libs...>])
#
# 把一个真实 Krita 测试类（<testbase>.h / <testbase>.cpp）**零改动**跑在 pk 栈上：
#   1. pk_test_moc.py 从测试头生成 PkTestBinder<T> 特化（替代 moc）；
#   2. 生成一个 wrap TU：compat/QObject → compat/QTest → 测试头 → binder → 测试源
#      （brief §2 配方 4 的「wrap TU」形态 —— **零测试源改动**，binder 落在类声明
#      之后，天然满足顺序）；
#   3. 注册转调 kis_add_test(TEST_NAME <testbase> ...)：于是 add_test 名、NAME_PREFIX、
#      KRITA_TESTS_TARGET、KRITA_BROKEN_TESTS、set_test_sdk_compile_definitions
#      （FILES_DATA_DIR 等）全部继承，测试名与基线逐字一致（配方 5）；
#   4. 转调之后关 AUTOMOC（pk 的 compat/QObject 没有元对象，moc_*.cpp 必然编不过）。
# -----------------------------------------------------------------------------
function(pk_add_test testbase)
    cmake_parse_arguments(ARG "" "SRCDIR" "LINK_LIBRARIES" ${ARGN})

    if(ARG_SRCDIR)
        set(_srcdir "${ARG_SRCDIR}")
    else()
        set(_srcdir "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()

    set(_header "${_srcdir}/${testbase}.h")
    set(_source "${_srcdir}/${testbase}.cpp")
    set(_binder "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_${testbase}.inc")
    set(_wrap "${CMAKE_CURRENT_BINARY_DIR}/pk_wrap_${testbase}.cpp")
    set(_moc_script "${CMAKE_SOURCE_DIR}/pk/test/pk_test_moc.py")

    if(NOT EXISTS "${_header}")
        message(FATAL_ERROR "pk_add_test(${testbase}): 测试头不存在：${_header}")
    endif()
    if(NOT EXISTS "${_source}")
        message(FATAL_ERROR "pk_add_test(${testbase}): 测试源不存在：${_source}")
    endif()

    # 函数作用域内 find_package：Python3_EXECUTABLE 只在 pk/ 作用域设过，兄弟目录
    # （libs/**）读不到。脚本本身带 shebang 可执行，但显式带上解释器更稳。
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    add_custom_command(
        OUTPUT "${_binder}"
        COMMAND "${Python3_EXECUTABLE}" "${_moc_script}" "${_header}" -o "${_binder}"
        DEPENDS "${_header}" "${_moc_script}"
        COMMENT "pk_test_moc ${testbase}"
        VERBATIM)

    # wrap TU 在 configure 期写盘（内容全是固定路径字符串），与壳 add_shell_test
    # 的 file(WRITE) 同形。
    file(WRITE "${_wrap}"
"// R-65 生成：pk 栈 wrap TU（零测试源改动）。
// 顺序照 brief §2 配方 4：compat/QObject -> compat/QTest -> 测试头 -> binder -> 测试源。
#include <pk/test/compat/QObject>
#include <pk/test/compat/QTest>
#include \"${_header}\"
#include \"${_binder}\"
#include \"${_source}\"
")

    kis_add_test("${_wrap}"
        TEST_NAME "${testbase}"
        LINK_LIBRARIES ${ARG_LINK_LIBRARIES} kritatestsdk_pk
    )

    # pk 的 compat/QObject 没有元对象，AUTOMOC 生成的 moc_*.cpp 必然编不过。
    set_target_properties(${testbase} PROPERTIES AUTOMOC OFF)
    target_compile_definitions(${testbase} PRIVATE KRITA_TESTSDK_PK_NATIVE)
    # binder 所在处（build 目录）与测试源目录（<simpletest.h> 等依赖由 kritatestsdk_pk
    # 提供，这里补测试源自身的同级头 include）。
    target_include_directories(${testbase} PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${_srcdir}")
    set_source_files_properties("${_wrap}" PROPERTIES OBJECT_DEPENDS "${_binder}")

    # 让下游按 target 名引用；同时把 pk 依赖回灌到 kritatestsdk_pk 之外无需要。
    set(${testbase}_PK_ADDED TRUE PARENT_SCOPE)
endfunction()

# -----------------------------------------------------------------------------
# pk_add_tests(<testbase>... [SRCDIR <dir>] [LINK_LIBRARIES <libs...>])
# 批量版本：对每个 basename 调一次 pk_add_test，共享 SRCDIR / LINK_LIBRARIES。
# -----------------------------------------------------------------------------
function(pk_add_tests)
    cmake_parse_arguments(ARG "" "SRCDIR" "LINK_LIBRARIES" ${ARGN})
    set(_extra)
    if(ARG_SRCDIR)
        list(APPEND _extra SRCDIR "${ARG_SRCDIR}")
    endif()
    if(ARG_LINK_LIBRARIES)
        list(APPEND _extra LINK_LIBRARIES ${ARG_LINK_LIBRARIES})
    endif()
    foreach(_testbase ${ARG_UNPARSED_ARGUMENTS})
        pk_add_test("${_testbase}" ${_extra})
    endforeach()
endfunction()
