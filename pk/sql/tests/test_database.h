#pragma once
#include <QObject>      // → pk/test/compat/QObject，提供 QObject/Q_OBJECT/Q_SLOTS
#include <PkTest.h>

// R-17 Task 1：PkSqlDatabase 单连接门面 + §0 P3（事务/嵌套事务失败）的钉子
// 测试。PkSqlDatabase 是本任务架构上的"单一全局默认连接"（见类注释），
// init()/cleanup() 负责在每个测试函数前后重建/关闭这个全局连接，避免
// 测试函数之间通过共享单例互相污染状态。
class TestDatabase : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void openIsOpenClose();
    void databaseOpenFalseDoesNotCloseAlreadyOpenConnection();
    void connectionNamesReflectsAddDatabase();
    void tablesListsUserTablesOnly();

    void transactionCommitSucceedsWithNoError();
    void nestedTransactionFailsWithTransactionError();
};
