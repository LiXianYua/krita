#pragma once
#include <QObject>      // → pk/test/compat/QObject，提供 QObject/Q_OBJECT/Q_SLOTS
#include <PkTest.h>

class PkThreadCallQueueSelfTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testCurrentThreadIdDiffersAcrossThreads();
    void testMainThreadRegistration();
    void testPostDefersToTargetThreadPump();
    void testPostRoutesToRealWorkerThread();
    void testPostBlockingWaitsForExecution();
};
