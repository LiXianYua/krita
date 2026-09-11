#include "test_query.h"

#include "../PkSqlDatabase.h"
#include "../PkSqlError.h"
#include "../PkSqlQuery.h"
#include "../PkSqlCursor.h"

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

void TestQuery::namedValueLookupResolvesQualifiedNames()
{
    // Q-10：PkSqlQuery::value("table.field") 必须与 QSqlRecord::indexOf()
    // 等价——限定名要能查到列，且落**位对应的那一列**，不是第一个同名列。
    // Qt 依据：qtbase/src/sql/kernel/qsqlrecord.cpp:233-255（:248-251 的
    // `currentField.tableName().compare(tableName)`），表名来源见
    // src/plugins/sqldrivers/sqlite/qsql_sqlite.cpp:205-250。
    sqlite3_exec(PkSqlDatabase::database().PkHandle(),
                 "CREATE TABLE tags (id INTEGER PRIMARY KEY, name TEXT);"
                 "CREATE TABLE resource_types (id INTEGER PRIMARY KEY, name TEXT);"
                 "INSERT INTO tags (id, name) VALUES (11, 'tagname');"
                 "INSERT INTO resource_types (id, name) VALUES (22, 'brush');",
                 nullptr, nullptr, nullptr);

    PkSqlQuery q;
    PK_VERIFY(q.exec("SELECT tags.id, tags.name, resource_types.id, resource_types.name "
                     "FROM tags, resource_types"));
    PK_VERIFY(q.next());

#ifdef PK_SQLITE_HAS_COLUMN_METADATA
    // 表名可得：限定名各自落到自己表那一列（resource_types.id 是列 2，不是列 0）
    PK_COMPARE(q.value(PkString("tags.id")).toInt(), 11);
    PK_COMPARE(q.value(PkString("resource_types.id")).toInt(), 22);
    PK_COMPARE(q.value(PkString("tags.name")).toString(), PkString("tagname"));
    PK_COMPARE(q.value(PkString("resource_types.name")).toString(), PkString("brush"));
    // 大小写无关（Qt 用 Qt::CaseInsensitive）
    PK_COMPARE(q.value(PkString("TAGS.ID")).toInt(), 11);
    // 表名对不上 ⇒ 查不到（不是「退化成第一个同名列」）
    PK_VERIFY(!q.value(PkString("storages.id")).isValid());
#else
    // 拿不到表名（未编 SQLITE_ENABLE_COLUMN_METADATA）的降级语义：切掉
    // "table." 前缀后只比列名 ⇒ "resource_types.id" 退化成第一个 "id"。
    // 这条降级语义的**独立覆盖**在
    // cursorQualifiedLookupWithoutTableMetadataFallsBackToFieldName()。
    PK_COMPARE(q.value(PkString("tags.id")).toInt(), 11);
    PK_COMPARE(q.value(PkString("resource_types.id")).toInt(), 11);
    PK_COMPARE(q.value(PkString("tags.name")).toString(), PkString("tagname"));
    // 降级路径同样分不清同名列：也落回第一个 "name"（列 1，tags.name）
    PK_COMPARE(q.value(PkString("resource_types.name")).toString(), PkString("tagname"));
#endif

    // 裸名：第一个同名列（列 0），两条路径下一致
    PK_COMPARE(q.value(PkString("id")).toInt(), 11);
    // 列名对不上 ⇒ 查不到，两条路径下一致
    PK_VERIFY(!q.value(PkString("tags.filename")).isValid());

    // 单表场景（记录只来自一张表、无重名歧义）：限定名与裸名都给同一个列
    PkSqlQuery single;
    PK_VERIFY(single.exec("SELECT tags.id, tags.name FROM tags"));
    PK_VERIFY(single.next());
    PK_COMPARE(single.value(PkString("tags.id")).toInt(), 11);
    PK_COMPARE(single.value(PkString("tags.name")).toString(), PkString("tagname"));
    PK_COMPARE(single.value(PkString("id")).toInt(), 11);
#ifdef PK_SQLITE_HAS_COLUMN_METADATA
    PK_VERIFY(!single.value(PkString("resource_types.id")).isValid());
#else
    // 降级路径下 resource_types.id 切出 "id"，命中唯一那一列
    PK_COMPARE(single.value(PkString("resource_types.id")).toInt(), 11);
#endif
}

void TestQuery::cursorQualifiedLookupMatchesOwningTable()
{
    // 直接对 PkSqlCursor 造「表名可得」的状态（列名与表名一一对应），
    // 等价于编了 SQLITE_ENABLE_COLUMN_METADATA 的构建跑完 prepare() 后的形态。
    // 契约 = QSqlRecord::indexOf()（qsqlrecord.cpp:233-255）。
    PkSqlCursor c;
    std::vector<PkString> names;
    names.push_back(PkString("id"));    // tags.id
    names.push_back(PkString("name"));  // tags.name
    names.push_back(PkString("id"));    // resource_types.id
    names.push_back(PkString("name"));  // resource_types.name
    c.setColumnNames(names);

    std::vector<PkString> tables;
    tables.push_back(PkString("tags"));
    tables.push_back(PkString("tags"));
    tables.push_back(PkString("resource_types"));
    tables.push_back(PkString("resource_types"));
    c.setColumnTables(tables);

    // 限定名落到位对应的列，不是第一个同名列
    PK_COMPARE(c.columnIndex(PkString("tags.id")), 0);
    PK_COMPARE(c.columnIndex(PkString("tags.name")), 1);
    PK_COMPARE(c.columnIndex(PkString("resource_types.id")), 2);
    PK_COMPARE(c.columnIndex(PkString("resource_types.name")), 3);
    // 裸名：第一个同名列
    PK_COMPARE(c.columnIndex(PkString("id")), 0);
    PK_COMPARE(c.columnIndex(PkString("name")), 1);
    // 大小写无关
    PK_COMPARE(c.columnIndex(PkString("TAGS.ID")), 0);
    PK_COMPARE(c.columnIndex(PkString("Resource_Types.Id")), 2);
    // 表名或列名对不上 ⇒ -1
    PK_COMPARE(c.columnIndex(PkString("storages.id")), -1);
    PK_COMPARE(c.columnIndex(PkString("tags.filename")), -1);
    PK_COMPARE(c.columnIndex(PkString("nope")), -1);
}

void TestQuery::cursorQualifiedLookupWithoutTableMetadataFallsBackToFieldName()
{
    // 拿不到表名的构建（未编 SQLITE_ENABLE_COLUMN_METADATA）：不调
    // setColumnTables() ⇒ 降级为「在第一个 '.' 处切分、只比 fieldName」。
    PkSqlCursor c;
    std::vector<PkString> names;
    names.push_back(PkString("id"));    // tags.id
    names.push_back(PkString("name"));  // tags.name
    names.push_back(PkString("id"));    // resource_types.id
    names.push_back(PkString("name"));  // resource_types.name
    c.setColumnNames(names);

    // 降级后无法区分同名列：一律命中第一个同名列
    PK_COMPARE(c.columnIndex(PkString("tags.id")), 0);
    PK_COMPARE(c.columnIndex(PkString("resource_types.id")), 0);
    PK_COMPARE(c.columnIndex(PkString("tags.name")), 1);
    PK_COMPARE(c.columnIndex(PkString("resource_types.name")), 1);
    // 裸名行为与表名可得时一致
    PK_COMPARE(c.columnIndex(PkString("id")), 0);
    PK_COMPARE(c.columnIndex(PkString("name")), 1);
    // 大小写无关在这条路径下同样成立
    PK_COMPARE(c.columnIndex(PkString("TAGS.ID")), 0);
    // 列名对不上仍是 -1（切前缀不是「无脑命中最前面的列」）
    PK_COMPARE(c.columnIndex(PkString("tags.filename")), -1);
    PK_COMPARE(c.columnIndex(PkString("tags.nope")), -1);
    PK_COMPARE(c.columnIndex(PkString("nope")), -1);
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

void TestQuery::execBatchNamedValuesAsRowsInsertsAllRows()
{
    // R-17 Task 3 探针 [5]（task-3-report.md）：具名批量
    // bindValue(":name", PkVariantList) + execBatch()，"第 i 行取每个绑定
    // 的第 i 个元素"。真实 Qt 驱动 ok=true，numRowsAffected()=最后一行的
    // 影响行数（不是跨行累加——探针 [1]/[5] 都是这个结论）。
    PkSqlQuery q;
    PK_VERIFY(q.prepare("INSERT INTO t (id, name) VALUES (:id, :name)"));
    PkVariantList ids;
    ids.push_back(PkVariant(20));
    ids.push_back(PkVariant(21));
    PkVariantList names;
    names.push_back(PkVariant("twenty"));
    names.push_back(PkVariant("twentyone"));
    q.bindValue(":id", ids);
    q.bindValue(":name", names);
    PK_VERIFY(q.execBatch());
    PK_COMPARE(q.numRowsAffected(), 1); // 最后一行（第 2 行）的影响行数

    PkSqlQuery check;
    PK_VERIFY(check.exec("SELECT COUNT(*) FROM t WHERE id IN (20, 21)"));
    PK_VERIFY(check.next());
    PK_COMPARE(check.value(0).toInt(), 2);
}

void TestQuery::execBatchPositionalValuesAsRowsMatchesDeleteStorageShape()
{
    // 核对形态：libs/resources/KisResourceCacheDb.cpp:1817-1840
    // `deleteStorage()` 三处真实调用——单个位置占位符 `?` +
    // addBindValue(QVariantList) + execBatch()。
    PkSqlQuery seed;
    PK_VERIFY(seed.exec("INSERT INTO t (id, name) VALUES (2, 'a'), (3, 'b'), (4, 'c')"));

    PkSqlQuery q;
    PK_VERIFY(q.prepare("DELETE FROM t WHERE id = ?"));
    PkVariantList ids;
    ids.push_back(PkVariant(2));
    ids.push_back(PkVariant(3));
    ids.push_back(PkVariant(4));
    q.addBindValue(ids);
    PK_VERIFY(q.execBatch());

    PkSqlQuery check;
    PK_VERIFY(check.exec("SELECT COUNT(*) FROM t"));
    PK_VERIFY(check.next());
    PK_COMPARE(check.value(0).toInt(), 0);
}

void TestQuery::execBatchStopsAtFirstFailingRowLikeRealQtDriver()
{
    // R-17 Task 3 探针 [2][3]（task-3-report.md）：批量中某一行违反主键
    // 唯一性时，execBatch() 整体返回 false，**停在第一条失败的行**（不
    // 继续跑剩余行），已成功的行保留在库里（没有隐式事务包裹），
    // lastError() 反映失败那一行的错误分类（约束冲突 → ConnectionError，
    // 同 §0 P1）。init() 建的 t 表 id 是 `INTEGER PRIMARY KEY
    // AUTOINCREMENT`，本身即强制唯一，不需要额外 UNIQUE 列。
    PkSqlQuery seed;
    PK_VERIFY(seed.exec("INSERT INTO t (id, name) VALUES (11, 'existing')"));

    PkSqlQuery q;
    PK_VERIFY(q.prepare("INSERT INTO t (id, name) VALUES (?, ?)"));
    PkVariantList ids;
    ids.push_back(PkVariant(10));
    ids.push_back(PkVariant(11)); // 与种子行主键冲突
    ids.push_back(PkVariant(12));
    PkVariantList names;
    names.push_back(PkVariant("ten"));
    names.push_back(PkVariant("eleven"));
    names.push_back(PkVariant("twelve"));
    q.addBindValue(ids);
    q.addBindValue(names);

    PK_VERIFY(!q.execBatch());
    PK_COMPARE(static_cast<int>(q.lastError().type()),
               static_cast<int>(PkSqlError::ConnectionError));

    PkSqlQuery check;
    // id=10：批量第一行插入成功；id=11：种子行，插入尝试失败、原值不变；
    // id=12：批量第三行从未被跑到（第二行失败即停）。
    PK_VERIFY(check.exec("SELECT id, name FROM t WHERE id IN (10, 11, 12) ORDER BY id"));
    PK_VERIFY(check.next());
    PK_COMPARE(check.value(0).toInt(), 10);
    PK_COMPARE(check.value(1).toString(), PkString("ten"));
    PK_VERIFY(check.next());
    PK_COMPARE(check.value(0).toInt(), 11);
    PK_COMPARE(check.value(1).toString(), PkString("existing")); // 种子行未被覆盖
    PK_VERIFY(!check.next());                                    // id=12 从未插入
}

void TestQuery::singleArgConstructorPreparesWithoutExecuting()
{
    // R-17 plan §0 末尾订正（Task 2 探针）+ Task 3 落地：单参构造函数只
    // prepare()，不 exec()——`KisResourceCacheDb::addStorageType()` 唯一
    // 真实调用点的形态（构造后紧跟 addBindValue + 显式 exec()）。
    PkSqlQuery q(PkString("INSERT INTO t (name) VALUES (?)"));
    PK_VERIFY(q.lastQuery() == PkString("INSERT INTO t (name) VALUES (?)"));
    // 构造之后、addBindValue/exec 之前：没有执行过，影响行数应为初始值。
    PK_COMPARE(q.numRowsAffected(), -1);

    q.addBindValue(PkVariant("kritaBundle"));
    PK_VERIFY(q.exec());
    PK_COMPARE(q.numRowsAffected(), 1);

    PkSqlQuery check;
    PK_VERIFY(check.exec("SELECT COUNT(*) FROM t"));
    PK_VERIFY(check.next());
    // 只插入了这一行——没有因为"构造即执行"的隐式尝试多插入一条 NULL 行。
    PK_COMPARE(check.value(0).toInt(), 1);
}

void TestQuery::boundValuesReturnsNamedBinds()
{
    // R-17 全分支评审 Important #1：boundValues() 之前根本不存在（40 处
    // qWarning() 诊断打印调用点会直接编译失败）。具名绑定场景：bindValue()
    // 之后 boundValues() 里能查到对应 key 的值。
    PkSqlQuery q;
    PK_VERIFY(q.prepare("INSERT INTO t (name) VALUES (:name)"));
    q.bindValue(":name", PkVariant("kritaBundle"));

    PkVariantMap bound = q.boundValues();
    auto it = bound.find(PkString(":name"));
    PK_VERIFY(it != bound.end());
    PK_COMPARE(it->second.toString(), PkString("kritaBundle"));
}

void TestQuery::boundValuesReturnsPositionalBindsByStringIndex()
{
    // 位置绑定场景：addBindValue() 之后 boundValues() 按调用顺序（第 0、1…个）
    // 以十进制字符串 key 存下，同样能被诊断打印遍历到。
    PkSqlQuery q;
    PK_VERIFY(q.prepare("INSERT INTO t (id, name) VALUES (?, ?)"));
    q.addBindValue(PkVariant(7));
    q.addBindValue(PkVariant("brushPreset"));

    PkVariantMap bound = q.boundValues();
    PK_COMPARE(bound.size(), static_cast<std::size_t>(2));
    auto it0 = bound.find(PkString("0"));
    auto it1 = bound.find(PkString("1"));
    PK_VERIFY(it0 != bound.end());
    PK_VERIFY(it1 != bound.end());
    PK_COMPARE(it0->second.toInt(), 7);
    PK_COMPARE(it1->second.toString(), PkString("brushPreset"));
}

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_query.inc"

int run_query_tests(int argc, char **argv)
{
    TestQuery tc;
    return PkTest::qExec(&tc, argc, argv);
}
