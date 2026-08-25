#ifndef SIMPLETEST_H
#define SIMPLETEST_H

#include <PkTest.h>
#include <PkThread.h>
#include <PkThreadCallQueue.h>

// SIMPLE_TEST_MAIN/SIMPLE_MAIN_IMPL：零 Qt 版测试入口。
//
// 原实现会创建一个 GUI 应用对象（含 locale 默认值 / 测试模式路径 / DPI /
// 键盘导航禁用等六件事），按 R-10 PkObject 线程模型改写：那个对象的作用是
// 界定"主线程"并驱动一个事件循环，这里由 PkThread::registerMainThread()
// （登记当前线程为主线程，KisImage 等对象 moveToThread(mainThreadId()) 转的
// 就是它）+ PkThreadCallQueue::warmUpCurrentThread()（为当前线程准备调用队列，
// 跨线程投递的 PkThreadCallQueue::post 才能落到这里）承接。qExec 之前的资源
// 目录 qputenv（EXTRA_RESOURCE_DIRS / KRITA_PLUGIN_PATH）归 S-00 处理；
// KisSynchronizedConnectionBase::setAutoModeForUnittestsEnabled 由 D-30 裁定删除。

#define SIMPLE_MAIN_IMPL(TestObject) \
    PkThread::registerMainThread(); \
    PkThreadCallQueue::warmUpCurrentThread(); \
    TestObject tc; \
    return PkTest::qExec(&tc, argc, argv);

#define SIMPLE_TEST_MAIN(TestObject) \
int main(int argc, char *argv[]) \
{ \
    SIMPLE_MAIN_IMPL(TestObject) \
}

#endif // SIMPLETEST_H
