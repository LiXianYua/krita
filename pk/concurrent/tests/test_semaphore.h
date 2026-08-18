#pragma once
#include <QObject>      // → pk/test/compat/QObject，提供 QObject/Q_OBJECT/Q_SLOTS
#include <PkTest.h>

class PkSemaphoreSelfTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testAcquireReleaseBlocks();
    void testTryAcquireImmediate();
    void testTryAcquireTimeout();
    void testTryAcquireNegativeTimeoutBlocksForever();
};
