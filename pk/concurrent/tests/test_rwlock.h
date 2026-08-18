#pragma once
#include <QObject>      // → pk/test/compat/QObject，提供 QObject/Q_OBJECT/Q_SLOTS
#include <PkTest.h>

class TestReadWriteLock : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void lockForReadWrite();
    void readLockerRaii();
    void writeLockerRaii();
    void readLockerUnlockRelock();
    void writeLockerUnlockRelock();
    void readLockerReadWriteLockAccessor();
    void writeLockerReadWriteLockAccessor();
    void multipleReaders();
    void writeLockExclusivity();
    void upgradeReadToWrite();
    void tryLockForReadWriteWhenFree();
    void tryLockForWriteFailsWhenReadHeld();
    void tryLockForReadFailsWhenWriteHeld();
    void recursionModeConstructorAcceptsNonRecursive();
};
