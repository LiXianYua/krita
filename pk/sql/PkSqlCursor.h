#pragma once

#include <vector>

#include "../string/PkString.h"
#include "../variant/PkVariant.h"

// PkSqlCursor —— PkSqlQuery 的内部结果集缓冲，支撑 seek(int) 任意行跳转
// （R-17 plan §0 P6：Qt 的 SQLite 驱动对非 forward-only 查询支持任意行 seek，
// 内部按需要把已见过的行缓冲住，这正是 KisAllResourcesModel 等 Model 类把
// `data(index, role)` 直接接到 `d->query.seek(index.row())` 的前提）。
//
// 本实现走比"照抄单向 sqlite3_stmt 游标"更简单也更稳的路线：**`PkSqlQuery`
// 在一次 `exec()` 内把整个结果集一次性物化进 `m_rows`**（`sqlite3_step` 循环
// 跑到 `SQLITE_DONE` 为止），之后 `next()`/`first()`/`seek()`/`at()`/`value()`
// 全部只操作这份内存缓冲，不再碰 `sqlite3_stmt`——任意行跳转因此是免费的
// （线级 spec「不预先优化」：先把 seek 语义做对，要不要换成惰性/部分缓冲留给
// benchmark 决定，这不是本任务的判据）。
//
// 位置语义对齐 Qt 的 QSql 行位置惯例：
//   -1                   = BeforeFirstRow（初始状态，未调用过 next()/first()/seek()，
//                           也是耗尽方向相反端的越界结果）
//   [0, rowCount()-1]     = 有效行
//   rowCount()            = AfterLastRow（next() 耗尽后的终止状态）
// isValid() == 当前位置落在 [0, rowCount()) 内——与 PkSqlQuery::isValid() 共用
// 这一份判定（§1 用量表：`isValid()` 语义是"游标当前是否落在一条有效记录上"，
// 不是"query 是否曾经成功 prepare/exec 过"）。
class PkSqlCursor
{
public:
    PkSqlCursor();

    // 完全重置：列名 + 行缓冲 + 位置全部清空。`PkSqlQuery::prepare()`/`clear()`
    // 用它——新的一次 prepare 意味着列结构（可能）整个变了，旧列名不能留着。
    void clear();

    // 只清行缓冲 + 位置，**保留列名**。`PkSqlQuery::exec()` 在"prepare 一次、
    // 循环 bindValue+exec"模式下重复调用时用它——同一个 prepared statement
    // 反复执行，列结构（`sqlite3_column_name` 的取值）不会变，只有行数据变。
    void clearRows();

    void setColumnNames(const std::vector<PkString> &names);

    // 与 m_columnNames 一一对应的**列所属表名**（`sqlite3_column_table_name`）。
    // 可选：只有 sqlite 编了 SQLITE_ENABLE_COLUMN_METADATA 的构建里
    // PkSqlQuery::prepare() 才会调它。不调（或传空 vector）⇒ 表名不可得 ⇒
    // columnIndex() 的限定名匹配降级为「只比切出来的列名」（语义差异见
    // pk/sql/README.md）。生命周期与 setColumnNames 一致：clear() 一并清掉，
    // clearRows() 不动。
    void setColumnTables(const std::vector<PkString> &tables);

    void appendRow(const PkVariantList &row);

    int rowCount() const;
    int columnCount() const;
    // 具名查找，**与 `QSqlRecord::indexOf()` 等价**（Qt 源码
    // qtbase/src/sql/kernel/qsqlrecord.cpp:233-255）：
    //   1. 先拿**整个 name** 去比列名（列名里真含 '.' 的别名走这条）
    //   2. 整名比不中、且 name 含 '.' 时，在**第一个** '.' 处切成
    //      tableName/fieldName，要求列的 fieldName 比中 **且** 列所属表名
    //      比中 tableName（`"a.b.c"` ⇒ table="a"、field="b.c"，与 Qt 一致）
    // 比较一律**大小写无关**（Qt 用 `Qt::CaseInsensitive`；Pk 侧用
    // `PkString::toLower()`——同一份 Unicode 大小写映射表）。多列都比中时取
    // **最小下标**（Qt 逐列循环的语义）。
    //
    // 第 2 条要求「列所属表名」可得，它来自 `sqlite3_column_table_name()`，
    // 只在 sqlite 编了 SQLITE_ENABLE_COLUMN_METADATA 时才存在。拿不到表名的
    // 构建（m_columnTables 与 m_columnNames 长度不一致）降级为
    // 「切前缀、只比 fieldName」——语义差异见 pk/sql/README.md。
    // 找不到返回 -1。
    int columnIndex(const PkString &name) const;

    bool next();
    bool first();
    bool seek(int index);
    int at() const;
    bool isValid() const;

    // 越界/未定位一律返回 Invalid 的 PkVariant（默认构造），不设错误、不抛
    // 异常——R-17 plan §0 P2 的钉子结论，next()/query 是否有 lastError 完全
    // 由 PkSqlQuery 自己管，PkSqlCursor 本身不知道"error"这个概念。
    PkVariant value(int col) const;

private:
    std::vector<PkString> m_columnNames;
    // 与 m_columnNames 一一对应；空（或长度不一致）= 本构建拿不到表名。
    std::vector<PkString> m_columnTables;
    std::vector<PkVariantList> m_rows;
    int m_pos;
};
