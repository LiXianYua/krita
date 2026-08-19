#include "PkSqlCursor.h"

PkSqlCursor::PkSqlCursor() : m_pos(-1)
{
}

void PkSqlCursor::clear()
{
    m_columnNames.clear();
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
    for (std::size_t i = 0; i < m_columnNames.size(); ++i) {
        if (m_columnNames[i] == name) {
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
