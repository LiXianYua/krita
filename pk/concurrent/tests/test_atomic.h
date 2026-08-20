#pragma once
#include <QObject>      // → pk/test/compat/QObject，提供 QObject/Q_OBJECT/Q_SLOTS
#include <PkTest.h>

class TestAtomic : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testDefaultConstruct();
    void testRefDeref();
    void testFetchAndAddOrdered();
    void testFetchAndStoreOrdered();
    void testTestAndSetOrderedSuccess();
    void testTestAndSetOrderedFailure();
    void testAtomicPointerBasics();
    void testAtomicPointerCAS();
    void testConcurrentRefCounting();
    void testConcurrentCAS();
    void testLoadStoreRelaxed();
    void testFetchAndAddRelaxedAcquire();
    void testAcquireReleaseSequencing();
    void testAtomicPointerLoadStoreRelaxedAcquireRelease();
    void testAtomicPointerFetchAndAddRelaxedAcquire();
    void testImplicitIntLoadAcquire();
    void testImplicitIntStoreRelease();
    void testRefSeqCst();
    void testDerefSeqCst();
    void testImplicitPointerLoadAcquire();
    void testImplicitPointerArrowAcquire();
    void testImplicitPointerStoreRelease();
};
