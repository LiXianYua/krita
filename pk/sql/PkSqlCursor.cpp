#include "PkSqlCursor.h"

namespace {

// Qt 的 indexOf() 用 Qt::CaseInsensitive 比列名与表名。PkString::toLower()
// 走的是 pk/string 的 Unicode 大小写映射表（PkString_query.cpp），与 Qt 的
// case-insensitive 比较同一量级，不是 ASCII-only 的近似。
// 先比原串：绝大多数命中都是精确匹配，省掉两次 toLower()。
bool pkNameEquals(const PkString &a, const PkString &b)
{
    return a == b || a.toLower() == b.toLower();
}

} // namespace

PkSqlCursor::PkSqlCursor() : m_pos(-1)
{
}

void PkSqlCursor::clear()
{
    m_columnNames.clear();
    m_columnTables.clear();
    m_rows.clear();
    m_pos = -1;
}

void PkSqlCursor::clearRows()
{
    m_rows.clear();
    m_pos = -1;
}

void PkSqlCursor::setColumnNames(const std::vector<PkString> &names)
{
    m_columnNames = names;
}

void PkSqlCursor::setColumnTables(const std::vector<PkString> &tables)
{
    m_columnTables = tables;
}

void PkSqlCursor::appendRow(const PkVariantList &row)
{
    m_rows.push_back(row);
}

int PkSqlCursor::rowCount() const
{
    return static_cast<int>(m_rows.size());
}

int PkSqlCursor::columnCount() const
{
    return static_cast<int>(m_columnNames.size());
}

int PkSqlCursor::columnIndex(const PkString &name) const
{
    // 切分规则照抄 Qt（qsqlrecord.cpp:236-241）：只在**第一个** '.' 处切，
    // 剩余部分整段算列名——"a.b.c" ⇒ tableName="a"、fieldName="b.c"。
    PkString tableName;
    PkString fieldName = name;
    const int idx = name.indexOf(PkString("."));
    if (idx != -1) {
        tableName = name.left(idx);
        fieldName = name.mid(idx + 1);
    }

    // 表名可得与否：只有 setColumnTables() 给的 vector 与列名一一对应才算数。
    // 长度为 0 且列名也为 0 时循环不执行，那个退化情形无影响。
    const bool tableNamesKnown = m_columnTables.size() == m_columnNames.size();

    for (std::size_t i = 0; i < m_columnNames.size(); ++i) {
        // Qt 的判据（qsqlrecord.cpp:248-251）：整名先比（列名真的带 '.' 的
        // 别名走这条），再比限定名切分后的 fieldName + 列所属表名。
        // 表名不可得时跳过表名那半条比较（降级路径）。
        if (pkNameEquals(m_columnNames[i], name)
            || (idx != -1
                && pkNameEquals(m_columnNames[i], fieldName)
                && (!tableNamesKnown || pkNameEquals(m_columnTables[i], tableName)))) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool PkSqlCursor::next()
{
    // 耗尽后再调 next()（§0 P2 [8] "third(耗尽后再调)=0"）：位置钉在
    // AfterLastRow（== rowCount()），不越界继续增长，也不需要每次都重算——
    // 保持幂等。
    if (m_pos + 1 >= rowCount()) {
        m_pos = rowCount();
        return false;
    }
    ++m_pos;
    return true;
}

bool PkSqlCursor::first()
{
    return seek(0);
}

bool PkSqlCursor::seek(int index)
{
    // 负下标：钉在 BeforeFirstRow，不是本任务判据覆盖的场景（§0 探针没有
    // 探过 seek(负数)），按"越界"统一处理，返回 false。
    if (index < 0) {
        m_pos = -1;
        return false;
    }
    if (index >= rowCount()) {
        m_pos = rowCount();
        return false;
    }
    m_pos = index;
    return true;
}

int PkSqlCursor::at() const
{
    return m_pos;
}

bool PkSqlCursor::isValid() const
{
    return m_pos >= 0 && m_pos < rowCount();
}

PkVariant PkSqlCursor::value(int col) const
{
    if (!isValid()) {
        return PkVariant();
    }
    const PkVariantList &row = m_rows[static_cast<std::size_t>(m_pos)];
    if (col < 0 || col >= static_cast<int>(row.size())) {
        return PkVariant();
    }
    return row[static_cast<std::size_t>(col)];
}
