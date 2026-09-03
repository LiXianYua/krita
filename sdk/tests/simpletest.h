#ifndef SIMPLETEST_H
#define SIMPLETEST_H

#include <PkTest.h>
#include <PkThread.h>
#include <PkThreadCallQueue.h>

#include <type_traits>

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
// 目录 qputenv（KRITA_PLUGIN_PATH）由本文件接入生成的测试配置；
// KisSynchronizedConnectionBase::setAutoModeForUnittestsEnabled 由 D-30 裁定删除。

namespace KritaTestSdk
{

template <typename TestObject>
int runSimpleTest(TestObject *test, int argc, char **argv)
{
    if constexpr (std::is_base_of<PkTestObject, TestObject>::value) {
        return PkTest::qExec(test, argc, argv);
    } else {
        // 未迁移的 Qt QObject fixture 仍走 QtTest；原实现会创建 GUI 应用对象
        // （事件循环 / 主线程界定），迁移时只补了 PkThread 主线程登记，漏了
        // QApplication，导致 QEventLoop/QSignalSpy 在无应用对象下死等。这里补回。
        QApplication app(argc, argv);
        return QTest::qExec(test, argc, argv);
    }
}

} // namespace KritaTestSdk

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
