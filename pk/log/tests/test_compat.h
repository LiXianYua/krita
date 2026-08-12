#pragma once
#include <QObject>      // → pk/test/compat/QObject，提供 QObject/Q_OBJECT/Q_SLOTS
#include <PkTest.h>

// Task 4 的核心断言：真的走 compat/QDebug 与 compat/QLoggingCategory 垫片
// （-I pk/log/compat），不是拿 Pk 名字抄一遍同样的测试。测试体本身在
// test_compat.cpp 顶部写 `#include <QDebug>` / `#include <QLoggingCategory>`。
class PkLogCompatTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    // Step 1 场景原样：QtInfoMsg 分类下 dbgTestKrita（debug 级）不产出、
    // warnTestKrita（warning 级）产出，且真实调用点形态的
    // `QDebug operator<<(QDebug, const Bar&)` 原样编过并按预期换行。
    void testCategoryGatesDebugButNotWarning();
    void testUserOperatorOverloadOnQDebugRoundTrips();

    // qDebug/qInfo/qWarning/qCritical：无分类版本，两种真实调用形态都要能编
    // 且行为对——流式 `qDebug() << ...` 与 printf 变参 `qWarning("%s", x)`。
    void testFreeFunctionStreamFormWorks();
    void testFreeFunctionPrintfFormWorks();

    // qPrintable：鸭子类型取 .PkToUtf8()，喂进 printf 变参形态的 qDebug。
    void testQPrintableExtractsUtf8ForPrintfArg();

    // qInstallMessageHandler：装/卸、返回旧 handler、装上之后收到消息。
    void testInstallMessageHandlerRoundTrips();

    // 评审 Critical 项：无分类（"default"）自由函数日志族必须真的到达 spdlog，
    // 不能只断言 sink 通道收到——sink 通道即使在旧的 bug 下也照样收到，掩盖了
    // spdlog 侧丢消息这件事（全部既有测试都只挂在 sink 通道上，这是这条 bug
    // 能躲过七轮评审的根因）。
    void testFreeFunctionLogReachesSpdlogNotJustSink();
};
