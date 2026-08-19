#pragma once
#include <QObject>      // → pk/test/compat/QObject，提供 QObject/Q_OBJECT/Q_SLOTS
#include <PkTest.h>

// R-17 Task 1：§0 P1 探针（错误分类）的钉子测试。PkSqlQuery 还没交付
// （Task 2 的范围），这里直接驱动裸 sqlite3 C API 走到 prepare/step 失败，
// 再喂给 PkSqlErrorFactory 验证分类结果——测的是分类逻辑本身，不依赖
// PkSqlQuery 的存在。
class TestError : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void noErrorIsInvalid();
    void syntaxErrorIsStatementError();
    void tableNotFoundIsStatementError();
    void prepareTimeSyntaxErrorIsStatementError();
    void uniqueConflictIsConnectionError();
    void primaryKeyConflictIsConnectionError();
    void notNullConflictIsConnectionError();
};
