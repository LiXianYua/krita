#pragma once
#include <QObject>      // → pk/test/compat/QObject，提供 QObject/Q_OBJECT/Q_SLOTS
#include <PkTest.h>

// R-17 Task 2：PkSqlQuery 核心——prepare/bind/exec/next/seek/value 的钉子测试。
// init()/cleanup() 同 TestDatabase：每个测试函数前后重建/关闭全局单例连接，
// 避免测试函数之间通过共享单例互相污染状态。
class TestQuery : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void namedBindValueInsertsRow();
    void positionalBindValueInsertsRow();
    void prepareOnceLoopBindExecReusesStatement();
    void execOneShotSelectReadsRows();

    void valueBeforeNextIsInvalidNoError();
    void valueAfterNextExhaustedIsInvalidNoError();
    void valueColumnIndexOutOfRangeIsInvalid();

    void sizeIsAlwaysMinusOneRegardlessOfForwardOnly();

    void seekJumpsBackwardAfterForwardOnlyNext();
    void seekJumpsBackwardAfterRandomAccessNext();

    void namedValueLookupIsUnqualifiedColumnNameOnly();

    void clearResetsToEmptyQueryReadyForReprepare();

    // R-17 Task 3：execBatch 两种批量绑定语义 + 单参构造函数。
    void execBatchNamedValuesAsRowsInsertsAllRows();
    void execBatchPositionalValuesAsRowsMatchesDeleteStorageShape();
    void execBatchStopsAtFirstFailingRowLikeRealQtDriver();
    void singleArgConstructorPreparesWithoutExecuting();
};
