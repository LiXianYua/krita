#include "test_query.h"

#include "../PkSqlDatabase.h"
#include "../PkSqlError.h"
#include "../PkSqlQuery.h"

#include <sqlite3.h>

void TestQuery::init()
{
    PkSqlDatabase db = PkSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(":memory:");
    PK_VERIFY(db.open());
    sqlite3_exec(db.PkHandle(), "CREATE TABLE t (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT)",
                 nullptr, nullptr, nullptr);
}

void TestQuery::cleanup()
{
    PkSqlDatabase::database(PkString(), false).close();
}

void TestQuery::namedBindValueInsertsRow()
{
    // §1 用量表：171 处具名占位符是 bindValue 的主形态。
    PkSqlQuery q;
    PK_VERIFY(q.prepare("INSERT INTO t (name) VALUES (:name)"));
    q.bindValue(":name", PkVariant("kritaBundle"));
    PK_VERIFY(q.exec());
    PK_COMPARE(q.numRowsAffected(), 1);
    PK_COMPARE(q.lastInsertId().toInt(), 1);

    PkSqlQuery check;
    PK_VERIFY(check.exec("SELECT id, name FROM t"));
    PK_VERIFY(check.next());
    PK_COMPARE(check.value(0).toInt(), 1);
    PK_COMPARE(check.value(1).toString(), PkString("kritaBundle"));
}

void TestQuery::positionalBindValueInsertsRow()
{
    // §1 用量表：13 处位置占位符 `?` + addBindValue。
    PkSqlQuery q;
    PK_VERIFY(q.prepare("INSERT INTO t (name) VALUES (?)"));
    q.addBindValue(PkVariant("brushPreset"));
    PK_VERIFY(q.exec());
    PK_COMPARE(q.numRowsAffected(), 1);

    PkSqlQuery check;
    PK_VERIFY(check.exec("SELECT name FROM t"));
    PK_VERIFY(check.next());
    PK_COMPARE(check.value(0).toString(), PkString("brushPreset"));
}

void TestQuery::prepareOnceLoopBindExecReusesStatement()
{
    // KisTagResourceModel::untagResources 的调用形态：prepare 一次，循环
    // bindValue+exec。每次 exec() 必须正确 reset 并按当前绑定值重新执行，
    // 不能把上一轮的行残留在结果缓冲里，也不能因为没有重新 prepare 而失败。
    PkSqlQuery q;
    PK_VERIFY(q.prepare("INSERT INTO t (name) VALUES (:name)"));
    const char *names[] = {"a", "b", "c"};
    for (int i = 0; i < 3; ++i) {
        q.bindValue(":name", PkVariant(names[i]));
        PK_VERIFY(q.exec());
        PK_COMPARE(q.numRowsAffected(), 1);
    }

    PkSqlQuery check;
    PK_VERIFY(check.exec("SELECT COUNT(*) FROM t"));
    PK_VERIFY(check.next());
    PK_COMPARE(check.value(0).toInt(), 3);
}

void TestQuery::execOneShotSelectReadsRows()
{
    // 一次性 exec(PkString)（无需先 prepare）之后仍可以 next() 读结果——
    // `KisSqlQueryLoader` 与真实调用点的形态。
    PkSqlQuery seed;
    PK_VERIFY(seed.exec("INSERT INTO t (name) VALUES ('x')"));
    PK_VERIFY(seed.exec("INSERT INTO t (name) VALUES ('y')"));

    PkSqlQuery q;
    PK_VERIFY(q.exec("SELECT name FROM t ORDER BY id"));
    PK_VERIFY(q.isSelect());
    int count = 0;
    while (q.next()) {
        ++count;
    }
    PK_COMPARE(count, 2);
}

void TestQuery::valueBeforeNextIsInvalidNoError()
{
    // §0 P2 [9b]：next() 从未被调用过时 value() 一律 invalid，不设错误。
    PkSqlQuery q;
    PK_VERIFY(q.exec("SELECT id FROM t")); // 空结果集
    PkVariant v = q.value(0);
    PK_VERIFY(!v.isValid());
    PK_COMPARE(static_cast<int>(q.lastError().type()), static_cast<int>(PkSqlError::NoError));
}

void TestQuery::valueAfterNextExhaustedIsInvalidNoError()
{
    // §0 P2 [8][9]：next() 序列 first=true / 耗尽=false / 耗尽后再调=false，
    // 耗尽后 value() 全部字段（isValid/isNull/toString/toInt）与"未定位"一致，
    // lastError() 保持 NoError（不是错误，是正常终止）。
    PkSqlQuery seed;
    PK_VERIFY(seed.exec("INSERT INTO t (name) VALUES ('only')"));

    PkSqlQuery q;
    PK_VERIFY(q.exec("SELECT name FROM t"));
    PK_VERIFY(q.next());  // first=true
    PK_VERIFY(!q.next()); // 耗尽=false
    PK_VERIFY(!q.next()); // 耗尽后再调=false

    PkVariant v = q.value(0);
    PK_VERIFY(!v.isValid());
    PK_VERIFY(v.isNull());
    PK_COMPARE(v.toString(), PkString());
    PK_COMPARE(v.toInt(), 0);
    PK_COMPARE(static_cast<int>(q.lastError().type()), static_cast<int>(PkSqlError::NoError));
}

void TestQuery::valueColumnIndexOutOfRangeIsInvalid()
{
    // §0 P2 [10]：列下标越界 isValid()=0 toString()=[]。
    PkSqlQuery seed;
    PK_VERIFY(seed.exec("INSERT INTO t (name) VALUES ('row')"));

    PkSqlQuery q;
    PK_VERIFY(q.exec("SELECT id, name FROM t"));
    PK_VERIFY(q.next());
    PkVariant v = q.value(99);
    PK_VERIFY(!v.isValid());
    PK_COMPARE(v.toString(), PkString());
}

void TestQuery::sizeIsAlwaysMinusOneRegardlessOfForwardOnly()
{
    // §0 P5：forwardOnly=true/false 两种情况 size() 都恒为 -1。
    PkSqlQuery seed;
    PK_VERIFY(seed.exec("INSERT INTO t (name) VALUES ('a')"));
    PK_VERIFY(seed.exec("INSERT INTO t (name) VALUES ('b')"));

    PkSqlQuery a;
    a.setForwardOnly(true);
    PK_VERIFY(a.exec("SELECT name FROM t"));
    PK_COMPARE(a.size(), -1);

    PkSqlQuery b;
    b.setForwardOnly(false);
    PK_VERIFY(b.exec("SELECT name FROM t"));
    PK_COMPARE(b.size(), -1);
}

void TestQuery::seekJumpsBackwardAfterForwardOnlyNext()
{
    // §0 P6 [C]：forwardOnly=true，next() 过一次后 seek(0) 仍然 = true
    // （返回当前行成功，不是"禁止"）。
    PkSqlQuery seed;
    PK_VERIFY(seed.exec("INSERT INTO t (name) VALUES ('row0')"));
    PK_VERIFY(seed.exec("INSERT INTO t (name) VALUES ('row1')"));

    PkSqlQuery q;
    q.setForwardOnly(true);
    PK_VERIFY(q.exec("SELECT name FROM t ORDER BY id"));
    PK_VERIFY(q.next());
    PK_VERIFY(q.seek(0));
    PK_COMPARE(q.value(0).toString(), PkString("row0"));
}

void TestQuery::seekJumpsBackwardAfterRandomAccessNext()
{
    // §0 P6 [D]：非 forwardOnly，next() 两次后 seek(0)（往回跳）= true，
    // value(0) 拿到第 0 行数据。
    PkSqlQuery seed;
    PK_VERIFY(seed.exec("INSERT INTO t (name) VALUES ('row0')"));
    PK_VERIFY(seed.exec("INSERT INTO t (name) VALUES ('row1')"));

    PkSqlQuery q;
    PK_VERIFY(q.exec("SELECT name FROM t ORDER BY id"));
    PK_VERIFY(q.next());
    PK_VERIFY(q.next());
    PK_VERIFY(q.seek(0));
    PK_COMPARE(q.value(0).toString(), PkString("row0"));
}

void TestQuery::namedValueLookupIsUnqualifiedColumnNameOnly()
{
    // brief 要求核对的一点：KisResourceLocator.cpp 的 value("tags.id")/
    // value("resource_types.id") 用的是带表前缀的限定名写法。本任务用真实
    // sqlite3 C API 探针核实过（task-2-report.md）：sqlite3_column_name
    // 报出的是**裸列名**（"id"），不带 "tags."/"resource_types." 前缀——
    // 两处限定名查找在真实 Qt 环境下也查不到列、返回 Invalid（且这两个局部
    // 变量在 KisResourceLocator.cpp 里赋值后从未被使用，是已存在的死代码，
    // 不是本类引入的偏差）。本类原样复刻：具名查找精确匹配裸列名。
    sqlite3_exec(PkSqlDatabase::database().PkHandle(),
                 "CREATE TABLE tags (id INTEGER PRIMARY KEY, name TEXT);"
                 "CREATE TABLE resource_types (id INTEGER PRIMARY KEY, name TEXT);"
                 "INSERT INTO tags (id, name) VALUES (1, 'tagname');"
                 "INSERT INTO resource_types (id, name) VALUES (2, 'brush');",
                 nullptr, nullptr, nullptr);

    PkSqlQuery q;
    PK_VERIFY(
        q.exec("SELECT tags.id, tags.name, resource_types.id FROM tags, resource_types"));
    PK_VERIFY(q.next());

    PK_VERIFY(!q.value(PkString("tags.id")).isValid());
    PK_VERIFY(!q.value(PkString("resource_types.id")).isValid());
    // 裸列名 "id" 命中第一个同名列（列 0，即 tags.id 那一列）。
    PK_COMPARE(q.value(PkString("id")).toInt(), 1);
    PK_COMPARE(q.value(PkString("name")).toString(), PkString("tagname"));
}

void TestQuery::clearResetsToEmptyQueryReadyForReprepare()
{
    // `KisAllResourcesModel::resetQuery()` 的形态：clear() 之后 isValid()
    // 必须是 false，且能在其上重新 prepare()/exec() 一次全新语句。
    PkSqlQuery q;
    PK_VERIFY(q.exec("SELECT name FROM t"));
    q.clear();
    PK_VERIFY(!q.isValid());
    PK_VERIFY(q.lastQuery().isEmpty());

    PK_VERIFY(q.prepare("SELECT id FROM t"));
    PK_VERIFY(q.exec());
}

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_query.inc"

int run_query_tests(int argc, char **argv)
{
    TestQuery tc;
    return PkTest::qExec(&tc, argc, argv);
}
