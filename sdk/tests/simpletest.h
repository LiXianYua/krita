#ifndef SIMPLETEST_H
#define SIMPLETEST_H

#include <PkTest.h>
#include <PkThread.h>
#include <PkThreadCallQueue.h>

#ifdef KRITA_TESTSDK_PK_NATIVE
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
