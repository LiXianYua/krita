#include "test_error.h"

#include "../PkSqlError.h"

#include <sqlite3.h>

namespace {

// 复刻 R-17 plan §0 P1 探针的建表语句：
// "CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE)"。
// 每个测试函数独立开一个 :memory: 连接（互不干扰，不共享状态）。
sqlite3 *openTestDb()
{
    sqlite3 *db = nullptr;
    sqlite3_open(":memory:", &db);
    sqlite3_exec(db, "CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE)",
                 nullptr, nullptr, nullptr);
    return db;
}

} // namespace

void TestError::noErrorIsInvalid()
{
    // 默认构造 = "无错误"：§0 P2 [8] "耗尽后 next()" 场景的对齐要求
    // ——不设错误、不抛异常，lastError() 保持 NoError。
    PkSqlError err;
    PK_COMPARE(static_cast<int>(err.type()), static_cast<int>(PkSqlError::NoError));
    PK_VERIFY(!err.isValid());
    PK_COMPARE(err.text(), PkString());
}

void TestError::syntaxErrorIsStatementError()
{
    // §0 P1 [2]：原始输出逐字对齐。
    sqlite3 *db = openTestDb();
    sqlite3_stmt *stmt = nullptr;
    const int rc = sqlite3_prepare_v2(db, "SELCT * FROM t", -1, &stmt, nullptr);
    PK_VERIFY(rc != SQLITE_OK);

    PkSqlError err = PkSqlErrorFactory::fromPrepareFailure(db, rc);
    PK_COMPARE(static_cast<int>(err.type()), static_cast<int>(PkSqlError::StatementError));
    PK_VERIFY(err.isValid());
    PK_COMPARE(err.nativeErrorCode(), PkString("1"));
    PK_COMPARE(err.databaseText(), PkString("near \"SELCT\": syntax error"));
    PK_COMPARE(err.driverText(), PkString("Unable to execute statement"));
    PK_COMPARE(err.text(), PkString("near \"SELCT\": syntax error Unable to execute statement"));

    if (stmt) {
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
}

void TestError::tableNotFoundIsStatementError()
{
    // §0 P1 [3]：语法错误与"表不存在"都归 SQLITE_ERROR，同一条分类路径。
    sqlite3 *db = openTestDb();
    sqlite3_stmt *stmt = nullptr;
    const int rc = sqlite3_prepare_v2(db, "SELECT * FROM nonexistent_table", -1, &stmt, nullptr);
    PK_VERIFY(rc != SQLITE_OK);

    PkSqlError err = PkSqlErrorFactory::fromPrepareFailure(db, rc);
    PK_COMPARE(static_cast<int>(err.type()), static_cast<int>(PkSqlError::StatementError));
    PK_COMPARE(err.nativeErrorCode(), PkString("1"));
    PK_COMPARE(err.databaseText(), PkString("no such table: nonexistent_table"));
    PK_COMPARE(err.driverText(), PkString("Unable to execute statement"));

    if (stmt) {
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
}

void TestError::prepareTimeSyntaxErrorIsStatementError()
{
    // §0 P1 [7]："prepare 期语法错误"——plan 只留了探针的结果（type=2、
    // nativeErrorCode=1、driverText 同 [2]，databaseText 大致是
    // "near \"VALUES\": syntax error"），没有留原始 SQL 源码。这里用一句
    // 自己构造的、列表末尾缺右括号就接 VALUES 的 INSERT 语句复现同一种
    // "prepare 直接失败"场景——sqlite 的列表解析器在 "name" 后既没等到
    // "," 也没等到 ")"，直接在 "VALUES" 这个 token 上报语法错误，实测
    // databaseText 正是 "near \"VALUES\": syntax error"，与 §0 P1 [7] 记录
    // 的原始结果吻合。（第一版用"末尾缺值列表"的写法——sqlite 对未终结的
    // 语句报的是 "incomplete input" 不是 "syntax error"，本地跑测试时
    // 已被抓出来订正，见 R-17 Task 1 报告。）
    sqlite3 *db = openTestDb();
    sqlite3_stmt *stmt = nullptr;
    const int rc = sqlite3_prepare_v2(db, "INSERT INTO t (id, name VALUES (5, 'x')", -1, &stmt,
                                       nullptr);
    PK_VERIFY(rc != SQLITE_OK);

    PkSqlError err = PkSqlErrorFactory::fromPrepareFailure(db, rc);
    PK_COMPARE(static_cast<int>(err.type()), static_cast<int>(PkSqlError::StatementError));
    PK_VERIFY(err.isValid());
    PK_COMPARE(err.nativeErrorCode(), PkString("1"));
    PK_COMPARE(err.driverText(), PkString("Unable to execute statement"));
    PK_VERIFY(err.databaseText().contains(PkString("syntax error")));

    if (stmt) {
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
}

void TestError::uniqueConflictIsConnectionError()
{
    // §0 P1 [4]：反直觉结论——UNIQUE 冲突分类成 ConnectionError，不是
    // StatementError。prepare 本身成功（语句合法），失败发生在 step。
    sqlite3 *db = openTestDb();
    sqlite3_exec(db, "INSERT INTO t (id, name) VALUES (1, 'foo')", nullptr, nullptr, nullptr);

    sqlite3_stmt *stmt = nullptr;
    PK_VERIFY(sqlite3_prepare_v2(db, "INSERT INTO t (id, name) VALUES (2, 'foo')", -1, &stmt,
                                  nullptr)
              == SQLITE_OK);
    const int rc = sqlite3_step(stmt);
    PK_COMPARE(rc, SQLITE_CONSTRAINT);

    PkSqlError err = PkSqlErrorFactory::fromStepFailure(db, rc);
    PK_COMPARE(static_cast<int>(err.type()), static_cast<int>(PkSqlError::ConnectionError));
    PK_VERIFY(err.isValid());
    PK_COMPARE(err.nativeErrorCode(), PkString("19"));
    PK_COMPARE(err.databaseText(), PkString("UNIQUE constraint failed: t.name"));
    PK_COMPARE(err.driverText(), PkString("Unable to fetch row"));
    PK_COMPARE(err.text(), PkString("UNIQUE constraint failed: t.name Unable to fetch row"));

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

void TestError::primaryKeyConflictIsConnectionError()
{
    // §0 P1 [5]。
    sqlite3 *db = openTestDb();
    sqlite3_exec(db, "INSERT INTO t (id, name) VALUES (1, 'foo')", nullptr, nullptr, nullptr);

    sqlite3_stmt *stmt = nullptr;
    PK_VERIFY(sqlite3_prepare_v2(db, "INSERT INTO t (id, name) VALUES (1, 'bar')", -1, &stmt,
                                  nullptr)
              == SQLITE_OK);
    const int rc = sqlite3_step(stmt);
    PK_COMPARE(rc, SQLITE_CONSTRAINT);

    PkSqlError err = PkSqlErrorFactory::fromStepFailure(db, rc);
    PK_COMPARE(static_cast<int>(err.type()), static_cast<int>(PkSqlError::ConnectionError));
    PK_COMPARE(err.nativeErrorCode(), PkString("19"));
    PK_COMPARE(err.databaseText(), PkString("UNIQUE constraint failed: t.id"));
    PK_COMPARE(err.driverText(), PkString("Unable to fetch row"));

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

void TestError::notNullConflictIsConnectionError()
{
    // §0 P1 [6]：bindValue 绑 QVariant() null 触发 NOT NULL 冲突。
    sqlite3 *db = openTestDb();

    sqlite3_stmt *stmt = nullptr;
    PK_VERIFY(sqlite3_prepare_v2(db, "INSERT INTO t (id, name) VALUES (?, ?)", -1, &stmt, nullptr)
              == SQLITE_OK);
    sqlite3_bind_int(stmt, 1, 3);
    sqlite3_bind_null(stmt, 2);
    const int rc = sqlite3_step(stmt);
    PK_COMPARE(rc, SQLITE_CONSTRAINT);

    PkSqlError err = PkSqlErrorFactory::fromStepFailure(db, rc);
    PK_COMPARE(static_cast<int>(err.type()), static_cast<int>(PkSqlError::ConnectionError));
    PK_COMPARE(err.nativeErrorCode(), PkString("19"));
    PK_COMPARE(err.databaseText(), PkString("NOT NULL constraint failed: t.name"));
    PK_COMPARE(err.driverText(), PkString("Unable to fetch row"));

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_error.inc"

int run_error_tests(int argc, char **argv)
{
    TestError tc;
    return PkTest::qExec(&tc, argc, argv);
}
