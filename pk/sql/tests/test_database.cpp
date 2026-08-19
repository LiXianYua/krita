#include "test_database.h"

#include "../PkSqlDatabase.h"
#include "../PkSqlError.h"
#include "../PkSqlQuery.h"

#include <sqlite3.h>

void TestDatabase::init()
{
    PkSqlDatabase db = PkSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(":memory:");
    PK_VERIFY(db.open());
}

void TestDatabase::cleanup()
{
    // open=false：不要在收尾时意外触发一次重新打开又立刻关闭。
    PkSqlDatabase::database(PkString(), false).close();
}

void TestDatabase::openIsOpenClose()
{
    PkSqlDatabase db = PkSqlDatabase::database();
    PK_VERIFY(db.isValid());
    PK_VERIFY(db.isOpen());
    db.close();
    PK_VERIFY(!db.isOpen());
    // 重新打开，让 cleanup() 的 close() 仍然作用在一个已知状态上。
    PK_VERIFY(db.open());
}

void TestDatabase::databaseOpenFalseDoesNotCloseAlreadyOpenConnection()
{
    // §1 用量表：QSqlDatabase::database(connName, open=false) 用于"先查
    // 连接是否已存在且可用，不新开连接"（KisResourceCacheDb::createDatabase()
    // 的调用形态）。init() 已经 addDatabase()+open() 过——open=false 不应该
    // 把一个已经打开的连接关掉，也不应该让返回的对象变成 invalid。
    PkSqlDatabase probe = PkSqlDatabase::database(PkString(), false);
    PK_VERIFY(probe.isValid());
    PK_VERIFY(probe.isOpen());
}

void TestDatabase::connectionNamesReflectsAddDatabase()
{
    PkStringList names = PkSqlDatabase::connectionNames();
    PK_COMPARE(names.size(), 1);
    PK_COMPARE(names[0], PkString(PkSqlDatabase::defaultConnection));
}

void TestDatabase::tablesListsUserTablesOnly()
{
    // AUTOINCREMENT 会让 sqlite 额外建一张内部表 sqlite_sequence——用它验证
    // tables() 确实排除了 "sqlite_" 前缀的内部表，不是只是凑巧没有内部表。
    PkSqlDatabase db = PkSqlDatabase::database();
    sqlite3_exec(db.PkHandle(), "CREATE TABLE foo (id INTEGER PRIMARY KEY AUTOINCREMENT)",
                 nullptr, nullptr, nullptr);

    PkStringList t = db.tables();
    PK_COMPARE(t.size(), 1);
    PK_COMPARE(t[0], PkString("foo"));
}

void TestDatabase::transactionCommitSucceedsWithNoError()
{
    // §0 P3 [12] 前半段：transaction() 成功、插入一行、commit() 成功，
    // lastError() 之后是 NoError。
    PkSqlDatabase db = PkSqlDatabase::database();
    sqlite3_exec(db.PkHandle(), "CREATE TABLE t (id INTEGER PRIMARY KEY)", nullptr, nullptr,
                 nullptr);

    PK_VERIFY(db.transaction());
    sqlite3_exec(db.PkHandle(), "INSERT INTO t (id) VALUES (1)", nullptr, nullptr, nullptr);
    PK_VERIFY(db.commit());
    PK_COMPARE(static_cast<int>(db.lastError().type()), static_cast<int>(PkSqlError::NoError));
}

void TestDatabase::nestedTransactionFailsWithTransactionError()
{
    // §0 P3 [13]：嵌套 transaction()（已有一个进行中的事务时再调一次）
    // ——Qt 驱动自己用内部布尔标志拦的，不是真的调用 sqlite3，
    // nativeErrorCode() 必须是空串。这是本任务判据里"必须原样复刻"的
    // 反直觉结论之一。
    PkSqlDatabase db = PkSqlDatabase::database();

    PK_VERIFY(db.transaction());  // first=1
    PK_VERIFY(!db.transaction()); // second(嵌套)=0

    PkSqlError err = db.lastError();
    PK_COMPARE(static_cast<int>(err.type()), static_cast<int>(PkSqlError::TransactionError));
    PK_VERIFY(err.isValid());
    PK_COMPARE(err.text(),
               PkString("cannot start a transaction within a transaction Unable to begin transaction"));
    PK_COMPARE(err.nativeErrorCode(), PkString());

    PK_VERIFY(db.rollback()); // 清理掉第一个仍然进行中的事务
}

void TestDatabase::queryFailureDoesNotPropagateToConnectionLevelLastError()
{
    // R-17 全分支评审修复 Important #2——真实 Qt/QSQLITE 探针实测结论（见
    // pk/sql/README.md 追加节「query 失败是否传播进 db 级 lastError()」）：
    // QSqlQuery::exec() 失败**不会**写连接级 QSqlDatabase::lastError()——
    // 探针 [A]/[C]/[D] 全部显示 db.lastError() 在 query 失败前后恒为
    // isValid()=false/type()=NoError，与 q.lastError() 各自独立。`pk/sql`
    // 现有实现（PkSqlQuery::execInternal() 只写自己的 m_lastError，从不碰
    // PkSqlDatabase 的单例状态）与这个真实结论一致，不需要改代码——这条测试
    // 钉住"不传播"这个结论本身，防止未来有人以为这是遗漏而"顺手"加上传播。
    PkSqlDatabase db = PkSqlDatabase::database();
    sqlite3_exec(db.PkHandle(), "CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE)",
                 nullptr, nullptr, nullptr);
    PK_COMPARE(static_cast<int>(db.lastError().type()), static_cast<int>(PkSqlError::NoError));

    // 语法错误：模拟 KisResourceCacheDb.cpp 428-436/450-457 行的调用形态
    // （`if (!q.exec()) { ...; return db.lastError(); }`）。
    PkSqlQuery q;
    PK_VERIFY(!q.exec("SELCT * FROM t"));
    PK_VERIFY(q.lastError().isValid());
    PK_COMPARE(static_cast<int>(db.lastError().type()), static_cast<int>(PkSqlError::NoError));
    PK_VERIFY(!db.lastError().isValid());

    // 约束冲突场景同样不传播。
    PkSqlQuery seed;
    PK_VERIFY(seed.exec("INSERT INTO t (id, name) VALUES (1, 'x')"));
    PkSqlQuery dup;
    PK_VERIFY(dup.prepare("INSERT INTO t (id, name) VALUES (?, ?)"));
    dup.addBindValue(PkVariant(1));
    dup.addBindValue(PkVariant("y"));
    PK_VERIFY(!dup.exec());
    PK_VERIFY(dup.lastError().isValid());
    PK_VERIFY(!db.lastError().isValid());
}

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_database.inc"

int run_database_tests(int argc, char **argv)
{
    TestDatabase tc;
    return PkTest::qExec(&tc, argc, argv);
}
