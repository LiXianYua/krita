#pragma once
#include <QObject>      // → pk/test/compat/QObject，提供 QObject/Q_OBJECT/Q_SLOTS
#include <PkTest.h>

class PkLogSinkTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testSinkReceivesMessage();
    void testRemovedSinkStopsReceiving();
    // 评审 Critical 项：sink 回调体内重入 PkLogRemoveSink / PkLogEmit
    // 不应死锁（std::mutex 非递归，分发时不能持锁回调）。
    void testSelfUnregisterDuringDispatchDoesNotDeadlock();
    void testEmitDuringDispatchDoesNotDeadlock();
    // 评审 Important 项：没调 PkLogEnsureLogger 就 PkLogEmit，不应静默兜底
    // 创建一个默认级别的 logger。
    void testEmitWithoutEnsureLoggerSkipsSilentFallback();
};
