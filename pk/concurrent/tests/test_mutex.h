#pragma once
#include <QObject>      // → pk/test/compat/QObject，提供 QObject/Q_OBJECT/Q_SLOTS
#include <PkTest.h>

class TestMutex : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void lockUnlock();
    void tryLock();
    void tryLockCamelCase();
    void mutexLockerRaii();
    void mutexLockerUnlockRelock();
    void mutexLockerMutexAccessor();
    void concurrentIncrement();
};
