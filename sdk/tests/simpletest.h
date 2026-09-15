#ifndef SIMPLETEST_H
#define SIMPLETEST_H

#include <PkTest.h>
#include <PkThread.h>
#include <PkThreadCallQueue.h>

#ifdef KRITA_TESTSDK_PK_NATIVE
// R-82：pk 栈也要 `KRITA_RESOURCE_DIRS_FOR_TESTS`（见下面
// KRITA_SIMPLE_TEST_RESOURCE_DIRS_SETUP 的说明）。
#include <KoTestConfig.h>
#include <cstdlib>

class QObject;
namespace QTest
{
int qExec(QObject *testObject, int argc, char **argv);
}
#else
#include <QTest>
#include <QApplication>
#include <KoTestConfig.h>
#endif

// R-77：pk 分支这里是**空**，这不是漏了。决策 D-12 之后插件层是静态注册、没有可加载的
// 插件 DSO（plugins/** 里 0 个 MODULE 目标，已由 S-09-g 落地），qputenv("KRITA_PLUGIN_PATH")
// 在任何栈上都不再生效——空掉它不会少给任何东西。原文「归 S0 处理」的归因已失效。
// 真需要注册表条目（如 KisFilterRegistry 的 "invert"）的测试，走既成的「静态链入 provider」
// 范式，不再走环境变量：样例是 plugins/impex/libkra/tests/CMakeLists.txt:22,26 的 -force_load /
// whole-archive，聚合器是 plugins/kritaplugin_registry/ 的 registerAllPlugins()。
// 锁外承接方：libs/image/tests/CMakeLists.txt——5 个 pk target 因 provider 未静态链入而在
// ctest 阶段 SegFault（kis_filter_test / kis_filter_mask_test / kis_async_merger_test /
// kis_adjustment_layer_test / kis_layer_test）；R-77 只报不改。
#ifdef KRITA_TESTSDK_PK_NATIVE
#define KRITA_SIMPLE_TEST_PLUGIN_PATH_SETUP
#else
#define KRITA_SIMPLE_TEST_PLUGIN_PATH_SETUP \
    qputenv("KRITA_PLUGIN_PATH", QByteArray(KRITA_PLUGINS_DIR_FOR_TESTS));
#endif

// ---------------------------------------------------------------------------
// R-82 · 资源目录环境变量（**这是 R-77 误删的一条**，与本文件上面的
// KRITA_PLUGIN_PATH 完全是两回事）。
//
// `libs/resources/KoResourcePaths.cpp:1384` 读的就是 `EXTRA_RESOURCE_DIRS`；
// Qt 栈的两个 `KISTEST_MAIN`（sdk/tests/kistest.h:381,400）都在建 QApplication 之前
// `qputenv("EXTRA_RESOURCE_DIRS", KRITA_RESOURCE_DIRS_FOR_TESTS)`。
// R-77 在 pk 分支把 `EXTRA_RESOURCE_DIRS` 与 `KRITA_PLUGIN_PATH` **一起**空掉了，
// 理由写的是「D-12 之后插件层是静态注册……该环境变量在**任何**栈上都不再生效」
// ——**那句话只对 `KRITA_PLUGIN_PATH` 成立**：`EXTRA_RESOURCE_DIRS` 与插件加载
// 毫无关系，它是 KoResourcePaths 的资源根目录清单（ICC 配色剖面就在
// `<KRITA_RESOURCE_DIRS_FOR_TESTS>` 的 `data/profiles/` 下）。
//
// 后果实测（R-82 Task 2 之后、本宏加入之前）：pk 栈的 impex 测试里
//   * `KoColorSpaceRegistry::rgb8()` 退化成 unmanaged 空间
//     （`KoSimpleColorSpace.h:110` 的 "Undefined operation … unmanaged"），
//     其 profile csId 为空 ⇒ `colorSpace(…, Float16, 该 profile)` 返回 **nullptr**
//     ⇒ exr / rgbe / heightmap / jxl / png / heif **6 条测试在导出路径上空指针解引用 SIGSEGV**；
//   * `kis_exr_test` 修好配色引擎后仍 `testFiles()` 失败 —— 逐像素比对
//     **98.34%（386686/393216 像素）不同**（512×768，真 Qt 探针实测），
//     与「参考图是 M0 的、今天的渲染不同」一致。
//
// **这与资源"注册"（registerResources()/KisTestUiResource 一族）不是一回事**，
// R-77 拒绝给后者一个空实现是对的（那会让资源测试静默跑绿）；本宏只声明
// **数据目录在哪**，不注册任何资源类型、不加载任何东西，不会制造假绿。
// ---------------------------------------------------------------------------
#ifdef KRITA_TESTSDK_PK_NATIVE
#define KRITA_SIMPLE_TEST_RESOURCE_DIRS_SETUP \
    setenv("EXTRA_RESOURCE_DIRS", KRITA_RESOURCE_DIRS_FOR_TESTS, 1);

// 引擎注册（R-82）：`pk_add_test(... REGISTER_ENGINES <符号…>)` 会为该 target
// 生成一个 `void pkRegisterTestEngines()`，这里**弱符号**调它。
//
// **必须在这里调、不能在静态初始化期调**：`registerLcmsEngine()` 经
// `KoResourcePaths::findAllAssets("icc_profiles", …)` 扫 ICC 剖面，而资源目录
// 刚由上一行 `KRITA_SIMPLE_TEST_RESOURCE_DIRS_SETUP` 设好；早于它调用会扫不到剖面，
// 且 `registerLcmsEngine()` 的 `static bool registered` 会挡住后续正确时机的调用
// （实测：`rgb8()` 退化成 fallback/线性剖面 ⇒ `testFiles()` 的渲染比参考图
// gamma 2.2 次幂偏暗、98.34% 像素不同）。对照 `sdk/smoke/paint_smoke.cpp:102-106,133,137`。
//
// **弱符号**是有意的：没选 `REGISTER_ENGINES` 的 target（例如零 Krita 库依赖的
// `PkTestSupportSelfTest`）不产生任何链接依赖，`if (…)` 判空即跳过。
// **强符号**调用，不是弱符号：实测 **Mach-O 上 `__attribute__((weak))` 声明不产生
// 弱引用**（ld 报强 undefined `_pkRegisterTestEngines`），要弱引用得用 Mach-O 的
// `weak_import`，那就要按平台分叉。改用「`pk_add_test` **无条件**生成一个定义
// `pkRegisterTestEngines()` 的 TU（没选 REGISTER_ENGINES 时函数体为空）」——
// 没有平台分叉，也不给任何一个 pk 目标引入 Krita 库依赖。
// 前提：pk 栈上的测试 target **都**由 pk_add_test 建（那是 pk 栈唯一的注册路径，
// `pk_add_tests` / `pk_add_benchmark` 都转调它）。
extern "C" void pkRegisterTestEngines();
#define KRITA_SIMPLE_TEST_ENGINE_SETUP \
    pkRegisterTestEngines();
#else
// Qt 栈的两个 KISTEST_MAIN 已经在建 QApplication 之前设过它（kistest.h:381,400），
// 这里再设一次等价、无副作用；留空以保持 Qt 栈行为逐字不变。
#define KRITA_SIMPLE_TEST_RESOURCE_DIRS_SETUP
#endif


// SIMPLE_TEST_MAIN/SIMPLE_MAIN_IMPL：过渡期双 Pk/Qt 测试入口。
// 默认分支保留 QtTest 给未迁移的 QObject fixture；只有
// KRITA_TESTSDK_PK_NATIVE 会抑制 QtTest include，供 Pk-native fixture 使用。
//
// 原实现会创建一个 GUI 应用对象（含 locale 默认值 / 测试模式路径 / DPI /
// 键盘导航禁用等六件事），按 R-10 PkObject 线程模型改写：那个对象的作用是
// 界定"主线程"并驱动一个事件循环，这里由 PkThread::registerMainThread()
// （登记当前线程为主线程，KisImage 等对象 moveToThread(mainThreadId()) 转的
// 就是它）+ PkThreadCallQueue::warmUpCurrentThread()（为当前线程准备调用队列，
// 跨线程投递的 PkThreadCallQueue::post 才能落到这里）承接。qExec 之前的资源
// 目录 qputenv（KRITA_PLUGIN_PATH）今日在任何栈上都已失效，见上方
// KRITA_SIMPLE_TEST_PLUGIN_PATH_SETUP 的 R-77 说明；
// KisSynchronizedConnectionBase::setAutoModeForUnittestsEnabled 由 D-30 裁定删除。

namespace KritaTestSdk
{

template <typename TestObject>
int runSimpleTest(TestObject *test, int argc, char **argv)
{
#ifdef KRITA_TESTSDK_PK_NATIVE
    return PkTest::qExec(test, argc, argv);
#else
    // 未迁移的 Qt QObject fixture 仍走 QtTest；原实现会创建 GUI 应用对象
    // （事件循环 / 主线程界定），迁移时只补了 PkThread 主线程登记，漏了
    // QApplication，导致 QEventLoop/QSignalSpy 在无应用对象下死等。这里补回。
    QApplication app(argc, argv);
    return QTest::qExec(test, argc, argv);
#endif
}

} // namespace KritaTestSdk

#ifdef KRITA_TESTSDK_PK_NATIVE
// <chrono>/<ctime> 只被下面的 PkTest::qWait 用；放在本 ifdef 内可让**真 Qt 测试栈**
// （kritatestsdk，不定义 KRITA_TESTSDK_PK_NATIVE）的 TU 前导与迁移前逐字节一致。
#include <chrono>
#include <ctime>

// ---------------------------------------------------------------------------
// R-65（Task 2+4）· `QTest::qWait` 的 pk 等价物
//
// 为什么在这里而不在 compat 垫片里：pk 栈上 `#define QTest PkTest`，测试源写
// `QTest::qWait(ms)` 就落到 `PkTest::qWait`。唯一的真实调用点
// sdk/tests/testutil.h 的 `TestUtil::MaskParent::waitForImageAndShapeLayers()`
// 在 do/while 里调它（等 KoShapeManager/KisShapeLayerCanvas 的 100ms 去抖）。
// 本函数**必须**放在 simpletest.h —— 它已经在上面 include 了 PkThreadCallQueue.h；
// 而若把 PkThreadCallQueue.h 放进 PkTestCompatAll.h（每个 TU 的**最前面** force-include），
// 会让 <mutex>/<thread>/<functional>/<memory> 成为 TU 里第一批标准头，实测把
// boost/operators.hpp:316 的 `::boost::addressof` 打崩（`no member named 'boost'
// in the global namespace`）。放在这里（simpletest.h 由被测头在**中段**拉入）不触发。
//
// 语义（真 Qt 探针 evidence/t24/probe_qpe-output.txt 的 D 组：QTest::qWait(200)
// 实测 elapsed=202ms）：等待约 ms 后返回，期间驱动事件循环。pk 栈没有隐式事件
// 循环，等价物是**显式 pump** PkThreadCallQueue::processPendingCalls()（R-24 的
// 投递原语；SIMPLE_MAIN_IMPL 也用它 warmUp）。逐字对齐「等到 deadline 为止、
// 期间反复抽干本线程调用队列」，**不是**简单 qSleep —— testutil.h:505-511 的注释
// 明写「不能简单换成 sleep（不会让挂起的 QTimer 触发，语义假绿）」。
//
// 已知偏离（登记）：真 Qt 的 qWait 还驱动 QPA 事件；pk 侧只有调用队列可驱动。
// 对唯一调用点而言这是同族的「显式同步 flush」，与注释里等的 S-08 交付同向，
// S-08 交付后本函数应删。
// ---------------------------------------------------------------------------
namespace PkTest {

inline void qWait(int ms)
{
    if (ms <= 0) {
        PkThreadCallQueue::processPendingCalls();
        return;
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    for (;;) {
        PkThreadCallQueue::processPendingCalls();
        if (std::chrono::steady_clock::now() >= deadline) break;
        struct timespec ts = { 0, 1000000 }; // 1ms
        nanosleep(&ts, nullptr);
    }
}

} // namespace PkTest
#endif

#define SIMPLE_MAIN_IMPL(TestObject) \
    KRITA_SIMPLE_TEST_PLUGIN_PATH_SETUP \
    KRITA_SIMPLE_TEST_RESOURCE_DIRS_SETUP \
    KRITA_SIMPLE_TEST_ENGINE_SETUP \
    PkThread::registerMainThread(); \
    PkThreadCallQueue::warmUpCurrentThread(); \
    TestObject tc; \
    return KritaTestSdk::runSimpleTest(&tc, argc, argv);

#define SIMPLE_TEST_MAIN(TestObject) \
int main(int argc, char *argv[]) \
{ \
    SIMPLE_MAIN_IMPL(TestObject) \
}

#endif // SIMPLETEST_H
