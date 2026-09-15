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
    # 2b) Pk 名落点目录（复用捐赠目录，见 impact-map.md §3.12）：
    #     PkXmlCompat.h 的激活清单写成 `#include PK_INC_(compat/PK_<N>_)`，当被激活的
    #     别名宏（pk/<dir>/compat/<裸名> 的映射）已生效时，include 源文本里的裸名被
    #     再扫描成 Pk 名 → 解析路径变成 `compat/Pk<N>`；pk 侧没有这个文件，报
    #     `fatal error: 'compat/PkColor' file not found`（实测落点
    #     libs/pigment/PkXmlCompat.h:45，经 libs/flake/PkFlakeBridge.h:743 进入 3 个
    #     flake/canvas 测试 TU）。libs/flake/flake/noqt-compat/compat/ 已有 32 个同款
    #     落点文件（其头注解释了「空实现与再 include 一次等价」），本行直接复用该目录
    #     而不复制文件。该目录只有 compat/ 子目录，不会遮蔽任何非 <compat/*> include。
    "${CMAKE_SOURCE_DIR}/libs/flake/flake/noqt-compat"
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
target_link_libraries(kritatestsdk_pk INTERFACE pktest pkconcurrent pkimageio)
# R-75：`pkimageio` 是 PkImage 文件 I/O（按路径构造 / load / save，R-75 Task 1 交付）
# 的符号落点——`PkImage.h` 声明它们、定义在 `pk/image/PkImageFileIo.cpp` 编进
# `pkimageio`（`pk/CMakeLists.txt` 的 `add_library(pkimageio SHARED …)`）。打开
# sdk/tests/qimage_test_util.h 的 checkQImage 族后，每一个 pk target 都会调用这三个
# 符号；不加则**所有** pk target 链接期缺符号（`Undefined symbols: PkImage::load…`）。
# `pkimageio` 是本仓库的 pk 库、不是 Krita 库，链它不破坏「零 Krita 库依赖 ⇒ 红了
# 就是机器坏了」这条对 sdk/tests 内 PkTestSupportSelfTest 的性质（该 target 仍编得过、
# 跑得绿，见 R-75 task-2 报告 §3.2）——但它是 SHARED，会在测试运行时被加载。

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
    # HEADERLESS（R-65 T2+4 新增）：测试类声明写在自己的 <testbase>.cpp 里、
    # 没有 <testbase>.h 的真实 Krita 测试（例：libs/canvas/tests/
    # kis_selection_tool_factory_test.cpp —— 类 KisSelectionToolFactoryTest 定义在
    # .cpp:53，末行 `#include "….moc"` 是 AUTOMOC 的落点）。此时 binder 的扫描输入
    # 就是 .cpp 自己（pk_test_moc.py 对 .h/.cpp 一视同仁，实测
    # `pk_test_moc.py <该 cpp> --stats` → `1  5`，5 个测试方法全列出），
    # wrap TU 不再单独 include 源文件（binder 自己会 include，避免无 include guard
    # 的 .cpp 被编两遍），并补一个空的 <testbase>.moc 占位（见下）。
    # BROKEN（R-82 新增）：把「破测试」语义也接过来。5 个 plugin target 在原树里是
    # `krita_add_broken_unit_test`（= KRITA_ADD_UNIT_TEST(... BROKEN)），而
    # KritaAddBrokenUnitTest.cmake 只在 `KRITA_ENABLE_BROKEN_TESTS` 时才 add_test
    # ⇒ 它们**本来不在 ctest 里**。R-82 把它们切到 pk 栈时若不转发这个位，ctest
    # 测试数会从 378 涨到 383（改变测试面），那是判据之外的变化。
    # kis_add_test 本来就是 KRITA_ADD_UNIT_TEST 的别名，而后者 parse 了 BROKEN
    # 选项 ⇒ 直接透传即可，不需要另写一条注册路径。
    cmake_parse_arguments(ARG "HEADERLESS;BENCHMARK;BROKEN" "SRCDIR;TEST_NAME;NAME_PREFIX;BENCHMARK_TARGET" "SOURCES;LINK_LIBRARIES;REGISTER_FILTERS;REGISTER_ENGINES" ${ARGN})

    if(ARG_SRCDIR)
        set(_srcdir "${ARG_SRCDIR}")
    else()
        set(_srcdir "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()

    # target / ctest 名。默认 = 文件 basename；TEST_NAME 用于「目标名 != 文件名」
    # 的真实 Krita 测试（如 kis_node_dummies_graph_test.cpp → KisNodeDummiesGraphTest）。
    if(ARG_TEST_NAME)
        set(_tgt "${ARG_TEST_NAME}")
    else()
        set(_tgt "${testbase}")
    endif()

    set(_source "${_srcdir}/${testbase}.cpp")
    if(ARG_HEADERLESS)
        set(_header "${_source}")
    else()
        set(_header "${_srcdir}/${testbase}.h")
    endif()
    set(_binder "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_${_tgt}.inc")
    set(_wrap "${CMAKE_CURRENT_BINARY_DIR}/pk_wrap_${_tgt}.cpp")
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
        COMMENT "pk_test_moc ${_tgt}"
        VERBATIM)

    # wrap TU 在 configure 期写盘（内容全是固定路径字符串），与壳 add_shell_test
    # 的 file(WRITE) 同形。
    if(ARG_HEADERLESS)
        # binder 的 .inc 自己 `#include "<绝对路径>/<testbase>.cpp"`，所以这里不再
        # 单独 include 源文件（该 .cpp 无 include guard，编两遍必然重定义）。
        file(WRITE "${_wrap}"
"// R-65 生成：pk 栈 wrap TU（HEADERLESS —— 测试类声明在自己的 .cpp 里）。
// 顺序：compat/QObject -> compat/QTest -> binder（binder 内部再 include 源文件）。
#include <pk/test/compat/QObject>
#include <pk/test/compat/QTest>
#include \"${_binder}\"
")
        # .cpp 末尾写死 `#include \"<testbase>.moc\"`（AUTOMOC 的落点）。pk 栈
        # 关 AUTOMOC、没有 moc 产物，该 include 会报 file not found；写一个空文件
        # 占位。真正的测试发现由上面的 binder 承担，本文件内容不参与编译语义。
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${testbase}.moc"
"// R-65：AUTOMOC 落点占位（空文件）。真实测试发现见 pk_binder_${_tgt}.inc。\n")
    else()
    file(WRITE "${_wrap}"
"// R-65 生成：pk 栈 wrap TU（零测试源改动）。
// 顺序照 brief §2 配方 4：compat/QObject -> compat/QTest -> 测试头 -> binder -> 测试源。
#include <pk/test/compat/QObject>
#include <pk/test/compat/QTest>
#include \"${_header}\"
#include \"${_binder}\"
#include \"${_source}\"
")
    endif()

    # -----------------------------------------------------------------------
    # REGISTER_FILTERS（R-82 新增）：把给定的 `extern "C" bool` 静态注册入口
    # **调用一次**，并借此把入口所在的归档成员按引用拉进链接。
    #
    # 为什么需要它（判据不是「编过」而是「跑绿」）：决策 D-12 之后插件是 STATIC +
    # 静态注册，注册表（libs/impex/KisImportExportManager.cpp:119-131 的**函数内
    # 静态表**）只被 `registerKisXxxFilter()` 写入。而链接器只按符号引用取归档成员
    # ⇒ 没人引用、更没人调用 ⇒ 表恒空 ⇒ `importDocument()` 一律返回
    # `FileFormatNotSupported`。此时断言「失败」的用例会**假绿**
    # （实测：KisTgaTest 的 testImportFromWriteonly / testExportToReadonly 在没有
    #  注册时照样 PASS，而断言**具体错误码**的 testImportIncorrectFormat 才把它
    #  暴露出来 —— 那一条正是本机制在收尾路径上的判别力对照物）。
    #
    # 形制取自 plugins/impex/tests/kis_impex_static_registration_test.cpp:20-60：
    # 它对每个 codec 的入口做 `extern "C" … KIS_WEAK_REGISTRATION` 声明并取地址，
    # 既拉成员又调用。那里是**全 42 个** codec 的 oracle；此处**按 target 窄取**
    # 该格式自己的那一两个入口（R-77 已裁定：不要把全部插件拖进每一个 pk 测试目标）。
    #
    # 生成的 TU 是 target 的**直接源文件**（不在归档里），所以它自己不会被丢弃，
    # 而它对外部入口的引用会把 `<archive>(member)` 拉进来 —— 于是不再需要
    # `-force_load <整档>`（整档形态在「同一个 .cpp 同时编进 import 与 export 两个
    # 归档」的 6 个格式上会撞 `duplicate symbol`，实测 tga）。
    # -----------------------------------------------------------------------
    # REGISTER_ENGINES 与 REGISTER_FILTERS 同形，只差签名：这些入口是
    # **C++ 链接的 `void f()`**（不是 `extern "C" bool f()`），典型是
    # `plugins/color/lcms2engine/LcmsEnginePlugin.cpp:70` 的 `registerLcmsEngine()`。
    #
    # 为什么 impex 测试需要它（R-82 实测的第二个 L2 实例）：D-12 之前配色引擎是
    # 运行时插件，每个测试都"自动"拿得到；静态化之后没人链它 ⇒
    # `KoColorSpaceRegistry::colorSpace(...)` 返回 **nullptr** ⇒ exr/rgbe/heightmap/jxl
    # 四条测试在导出路径上空指针解引用 SIGSEGV、psd 的 import 直接失败。
    # 它们**不是**接线错，是 pk 测试壳没把配色引擎拉起来 —— 与本文件顶部
    # REGISTER_FILTERS 那段是**同一个机理**（静态注册入口存在、没人链）。
    # `registerLcmsEngine()` 自带 `static bool registered` 幂等保护，且
    # `LcmsEnginePlugin.cpp:311-316` 另有一个匿名命名空间的静态对象做同样的事
    # ⇒ 拉进那个 .o 之后，显式调用与静态初始化**都不会重复生效**。
    # **无条件生成**（R-82）：即使既没有 REGISTER_FILTERS 也没有 REGISTER_ENGINES，
    # 也要产出一个定义 `pkRegisterTestEngines()`（空体）的 TU —— 因为
    # `SIMPLE_MAIN_IMPL` 会**强符号**调用它。
    # 为什么不用弱符号：实测 **Mach-O 上 `__attribute__((weak))` 声明不产生弱引用**
    # （ld 报强 undefined `_pkRegisterTestEngines`），`weak_import` 才是 Mach-O 的拼法，
    # 但那样又得按平台分叉；而「pk_add_test 无条件提供该函数」既没有平台分叉，
    # 又保住了零 Krita 库依赖的目标（空体函数，无任何外部依赖）。
    set(_register_src)
    if(TRUE)
        set(_engine_decls "")
        set(_engine_body "")
        foreach(_sym IN LISTS ARG_REGISTER_ENGINES)
            string(APPEND _engine_decls "void ${_sym}();\n")
            string(APPEND _engine_body "    ${_sym}();\n")
        endforeach()
        # R-82：**无条件**导出 `pkRegisterTestEngines()`（没有引擎时函数体为空）——
        # SIMPLE_MAIN_IMPL 是强符号调用它，所以每个 pk_add_test 目标都必须有定义。
            set(_engine_fn
"// R-82：引擎注册入口**刻意不在这里（静态初始化期）调用**。
// 它们依赖 KoResourcePaths 的资源目录（`registerLcmsEngine()` 经
// `KoResourcePaths::findAllAssets(\"icc_profiles\", …)` 扫 ICC 剖面），
// 而 `EXTRA_RESOURCE_DIRS` 是 SIMPLE_MAIN_IMPL 在 **main 里** 才设的
// ⇒ 静态初始化期调用会**赶在资源目录存在之前**跑完，而
// `registerLcmsEngine()` 自带 `static bool registered` 幂等保护，
// 后续正确时机的调用会被它挡掉 ⇒ 剖面扫不到、`rgb0()` 退化成 fallback 剖面。
// 实测对照：sdk/smoke/paint_smoke.cpp:102-106,133,137 就是「先 setenv、
// 再 registerLcmsEngine、再断言 sRGB-elle-V2-srgbtrc.icc 已注册」的正确顺序。
// 所以这里只**导出**一个函数，由 SIMPLE_MAIN_IMPL 在设好资源目录之后
// **强符号**调用它。没有引擎时函数体为空 ⇒ 空体函数不引入任何依赖，
// 「零 Krita 库依赖」的目标（PkTestSupportSelfSelfTest）照样成立。
extern \"C\" void pkRegisterTestEngines()
{
${_engine_body}}
")
    endif()
    if(TRUE)  # R-82：无条件生成（pkRegisterTestEngines 即使为空体也要有定义）
        set(_register_src "${CMAKE_CURRENT_BINARY_DIR}/pk_register_${_tgt}.cpp")
        set(_pk_decls "")
        set(_pk_calls "")
        foreach(_sym IN LISTS ARG_REGISTER_FILTERS)
            string(APPEND _pk_decls "extern \"C\" bool ${_sym}();\n")
            string(APPEND _pk_calls "        (void)${_sym}();\n")
        endforeach()
        file(WRITE "${_register_src}"
"// R-82 生成：pk 测试栈的**静态注册激活 TU**（由 pk_add_test 的 REGISTER_FILTERS 生成）。
// 两个作用，缺一不可：
//   1) 引用这些入口符号 ⇒ 链接器把含它们的归档成员拉进二进制
//      （否则 .o 整份被丢弃，构造函数/入口都不存在）；
//   2) 在静态初始化期**调用**它们 ⇒ KisImportExportManager 的注册表真的被填充。
// 顺序无关：注册表是函数内静态表（libs/impex/KisImportExportManager.cpp:121），
// 任何静态初始化期写入都安全；filter 本体由各入口的 lambda 惰性构造。
${_engine_decls}${_pk_decls}
${_engine_fn}
namespace {
struct PkImportExportRegistration {
    PkImportExportRegistration()
    {
${_pk_calls}    }
};
const PkImportExportRegistration pkImportExportRegistration;
} // namespace
")
    endif()

    # NAME_PREFIX：只在显式给了才转发（否则 kis_add_test 按路径自动推导，
    # 与基线一致）。libs/flake/flake/tests 这类目录的基线前缀是 libs-ui-，
    # 必须逐字转发，否则 ctest 名与基线不符。
    set(_nameprefix_arg)
    if(ARG_NAME_PREFIX)
        set(_nameprefix_arg NAME_PREFIX "${ARG_NAME_PREFIX}")
    endif()

    # ARG_SOURCES：额外源文件（如 sdk/tests/testutil.cpp），作为**独立 TU** 编进
    # 同一 target（wrap TU 只负责把 <testbase>.h/.cpp 合成一个 TU）。
    #
    # BENCHMARK 分支（R-65 T2+4 新增，供 pk_add_benchmark 用）：基准不是 ctest
    # 测试——krita_add_benchmark（cmake/modules/MacroKritaAddBenchmark.cmake）建的是
    # 可执行文件 + <测试名> custom target，且**不** add_test。所以这里走 add_executable
    # 而不是 kis_add_test（后者会注册 ctest 测试，改变测试计数）。其余步骤（wrap TU /
    # binder / 编译定义 / AUTOMOC OFF）与测试分支完全同源，机制不重复实现。
    if(ARG_BENCHMARK)
        add_executable("${_tgt}" "${_wrap}" ${ARG_SOURCES})
        target_link_libraries("${_tgt}" PRIVATE ${ARG_LINK_LIBRARIES} kritatestsdk_pk)
        ecm_mark_nongui_executable("${_tgt}")
        # 与 cmake/modules/KritaTestSuite.cmake 的 set_test_sdk_compile_definitions 同源：
        # kis_add_test 分支经 KritaAddBrokenUnitTest.cmake 已带上这两个定义，benchmark
        # 分支走 add_executable 拿不到 —— 补上（真实调用点：kis_low_memory_benchmark.cpp:56
        # 的 QString(FILES_DATA_DIR)，以及 FILES_OUTPUT_DIR 的测试输出目录口径）。
        target_compile_definitions("${_tgt}" PUBLIC FILES_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/")
        if(WIN32)
            target_compile_definitions("${_tgt}" PUBLIC FILES_OUTPUT_DIR="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
        else()
            target_compile_definitions("${_tgt}" PUBLIC FILES_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}")
        endif()
        if(ARG_BENCHMARK_TARGET)
            # 与 krita_add_benchmark 的 <测试名> custom target 同形：跑这个可执行文件。
            add_custom_target("${ARG_BENCHMARK_TARGET}" COMMAND $<TARGET_FILE:${_tgt}>)
            if(TARGET benchmark)
                add_dependencies(benchmark "${ARG_BENCHMARK_TARGET}")
            endif()
        endif()
    else()
        set(_broken_arg)
        if(ARG_BROKEN)
            set(_broken_arg BROKEN)
        endif()
        kis_add_test("${_wrap}" ${_register_src} ${ARG_SOURCES}
            TEST_NAME "${_tgt}"
            ${_nameprefix_arg}
            ${_broken_arg}
            LINK_LIBRARIES ${ARG_LINK_LIBRARIES} kritatestsdk_pk
        )
    endif()

    # pk 的 compat/QObject 没有元对象，AUTOMOC 生成的 moc_*.cpp 必然编不过。
    set_target_properties(${_tgt} PROPERTIES AUTOMOC OFF)
    target_compile_definitions(${_tgt} PRIVATE KRITA_TESTSDK_PK_NATIVE)
    # R-82：告诉 SIMPLE_MAIN_IMPL「本 target 有 pkRegisterTestEngines() 的定义」。
    # **不能只靠 KRITA_TESTSDK_PK_NATIVE 判**：pk 栈上还有**不走 pk_add_test** 的
    # 手写 target（实测：libs/pigment/tests、libs/psdutils/tests 的几个
    # `KRITA_TESTSDK_PK_NATIVE` 目标），它们没有这个定义 ⇒
    # 只按 KRITA_TESTSDK_PK_NATIVE 展开会得到 `undefined _pkRegisterTestEngines`
    # （全量闸门现场抓到，见 gate-ninja-full.log 的 TestKoColorSpaceRegistry /
    #  PkSimpleTestBridgeTest）。
    target_compile_definitions(${_tgt} PRIVATE PK_TEST_HAS_ENGINE_HOOK)
    # binder 所在处（build 目录）与测试源目录（<simpletest.h> 等依赖由 kritatestsdk_pk
    # 提供，这里补测试源自身的同级头 include）。
    target_include_directories(${_tgt} PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${_srcdir}")
    set_source_files_properties("${_wrap}" PROPERTIES OBJECT_DEPENDS "${_binder}")
    if(_register_src)
        set_source_files_properties("${_register_src}" PROPERTIES GENERATED TRUE)
    endif()

    set(${_tgt}_PK_ADDED TRUE PARENT_SCOPE)
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

# -----------------------------------------------------------------------------
# pk_add_benchmark(<目标名> TESTNAME <测试名> <testbase>
#                  [SRCDIR <dir>] [LINK_LIBRARIES <libs...>])
#
# krita_add_benchmark 的 pk 版（形制照 cmake/modules/MacroKritaAddBenchmark.cmake：
# 目标名 != 测试名）。用途：benchmarks/ 里那批用 SIMPLE_TEST_MAIN 的基准（如
# KisLowMemoryBenchmark / KisFilterSelectionsBenchmark）要跑在 pk 测试栈上。
#   * 目标名 = 可执行文件名 / CMake target 名（原宏的 _test_NAME）；
#   * <测试名> = custom target 名（原宏的 _targetName），add_dependencies(benchmark ...)；
#   * <testbase> = 测试类所在的源 basename（<testbase>.h / <testbase>.cpp），
#     wrap TU + binder 由 pk_add_test 的 BENCHMARK 分支生成。
# 通过 pk_add_test(… BENCHMARK …) 复用同一套 wrap/binder 机制，不另搓 add_executable。
# -----------------------------------------------------------------------------
function(pk_add_benchmark targetName)
    cmake_parse_arguments(ARG "" "TESTNAME;SRCDIR" "LINK_LIBRARIES" ${ARGN})
    list(LENGTH ARG_UNPARSED_ARGUMENTS _pk_n)
    if(_pk_n LESS 1)
        message(FATAL_ERROR "pk_add_benchmark(${targetName}): 缺少 <testbase>（测试类源 basename）")
    endif()
    list(GET ARG_UNPARSED_ARGUMENTS 0 _pk_testbase)
    set(_pk_extra)
    if(ARG_SRCDIR)
        list(APPEND _pk_extra SRCDIR "${ARG_SRCDIR}")
    endif()
    pk_add_test("${_pk_testbase}"
        TEST_NAME "${targetName}"
        BENCHMARK
        BENCHMARK_TARGET "${ARG_TESTNAME}"
        ${_pk_extra}
        LINK_LIBRARIES ${ARG_LINK_LIBRARIES})
endfunction()
