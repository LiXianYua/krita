#pragma once
#include <QObject>      // → pk/test/compat/QObject，提供 QObject/Q_OBJECT/Q_SLOTS
#include <PkTest.h>

class PkLogBackendConcurrencyTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    // task-9 缺陷回归：PkLogEnsureLogger 的 check-then-act 必须是原子的。
    void testConcurrentFirstUseOfCategoryDoesNotThrow();
};
