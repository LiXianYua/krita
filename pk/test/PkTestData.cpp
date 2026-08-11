#include "PkTestData.h"
#include "PkTestCase.h"
#include <cstdio>
#include <cstring>

PkTestDataRow::PkTestDataRow(PkTestTable *table, std::string tag)
    : m_table(table), m_tag(std::move(tag)), m_rowIndex(table->m_rows.size())
{
    table->m_rows.push_back(PkTestTable::Row{m_tag, {}});
}

void PkTestDataRow::appendValue(std::any value)
{
    // 入表阶段就校验列数与类型，与 Qt 一致（QTestData::append 对越界与类型不符都
    // 直接 qFatal）。写错的行在 _data() 里就被点名，而不是等到每一行调用
    // PK_FETCH 时才报一次——同一个错误报 N 次、且指向的是取值点不是建表点。
    PkTestTable::Row &row = m_table->m_rows[m_rowIndex];
    const std::size_t columnIndex = row.values.size();

    if (columnIndex >= m_table->m_columns.size()) {
        PkTestCase::current().recordFailure(
            "<PkTest::newRow>", 0,
            std::string("data row '") + m_tag + "' has more values than declared columns");
        return;
    }

    const PkTestTable::Column &column = m_table->m_columns[columnIndex];
    // 比对用 value.type().name()（std::any 内部已 decay 过的类型），不用调用方
    // 传来的 typeid(T).name()——后者是模板推导出的原始类型，数组字面量场景下
    // 与 std::any 实际存的类型不是同一个身份，会跟 valueAt() 的比对口径对不上
    // （valueAt() 比的就是 value.type().name()，见下方）。
    if (column.typeName != value.type().name()) {
        PkTestCase::current().recordFailure(
            "<PkTest::newRow>", 0,
            std::string("data row '") + m_tag + "': value type does not match column '"
                + column.name + "'");
        return;
    }

    row.values.push_back(std::move(value));
}

void PkTestTable::addColumn(const char *name, const char *typeName)
{
    m_columns.push_back(Column{name ? name : "", typeName ? typeName : ""});
}

PkTestDataRow &PkTestTable::newRow(const char *tag)
{
    // 存进 m_rowHandles 是为了让返回的引用活过这一条语句（`newRow(...) << a << b;`）。
    // 容器是 deque：它的 push_back 不使已有元素的引用失效，所以引用的有效性
    // 不依赖"调用点都在一条链式语句里用完"这条约定（见 PkTestData.h）。
    m_rowHandles.emplace_back(this, std::string(tag ? tag : ""));
    return m_rowHandles.back();
}

void PkTestTable::clear()
{
    m_columns.clear();
    m_rows.clear();
    m_rowHandles.clear();
}

const std::any *PkTestTable::valueAt(std::size_t rowIndex, const char *columnName,
                                     const char *typeName, std::string *outError) const
{
    for (std::size_t c = 0; c < m_columns.size(); ++c) {
        if (m_columns[c].name != columnName) {
            continue;
        }
        if (rowIndex >= m_rows.size() || c >= m_rows[rowIndex].values.size()) {
            if (outError) {
                *outError = std::string("data column '") + columnName + "' has no value for this row";
            }
            return nullptr;
        }
        const std::any &value = m_rows[rowIndex].values[c];
        if (std::strcmp(value.type().name(), typeName) != 0) {
            if (outError) {
                *outError = std::string("PK_FETCH type mismatch on column '") + columnName + "'";
            }
            return nullptr;
        }
        return &value;
    }
    if (outError) {
        *outError = std::string("no such data column: '") + columnName + "'";
    }
    return nullptr;
}

PkTestTable &PkTestTable::current()
{
    static PkTestTable instance;
    return instance;
}

namespace PkTest {

PkTestDataRow &newRow(const char *tag)
{
    return PkTestTable::current().newRow(tag);
}

PkTestDataRow &addRow(const char *format, ...)
{
    // 两遍法：第一遍只算长度，第二遍按精确长度格式化——避免固定缓冲区截断长 tag。
    va_list args;
    va_start(args, format);
    va_list argsForLength;
    va_copy(argsForLength, args);
    const int needed = std::vsnprintf(nullptr, 0, format, argsForLength);
    va_end(argsForLength);

    std::string tag;
    if (needed > 0) {
        tag.resize(static_cast<std::size_t>(needed));
        std::vsnprintf(tag.data(), tag.size() + 1, format, args);
    }
    va_end(args);

    return PkTestTable::current().newRow(tag.c_str());
}

const char *currentDataTag()
{
    // 按行号取，不用 PkTestCase 里那份 tag 副本：两者本该一致，但只留一条
    // 取值路径就不会有"副本漂移"这种可能。
    const std::size_t rowIndex = PkTestCase::current().currentDataRow();
    const PkTestTable &table = PkTestTable::current();
    if (rowIndex < table.rowCount()) {
        return table.tagAt(rowIndex).c_str();
    }
    return "";
}

const std::any *fetchData(const char *columnName, const char *typeName)
{
    // 按**行号**取值，不按 tag 反查。tag 可以重复（真实调用点见
    // libs/image/tests/TestAslStorage.cpp 的 testResource_data），线性反查会让
    // 重复 tag 的每一行都读到第一行的值；顺带也消掉了 O(行数×列数) 的查找。
    const std::size_t rowIndex = PkTestCase::current().currentDataRow();
    const PkTestTable &table = PkTestTable::current();

    std::string error;
    const std::any *value = nullptr;
    if (rowIndex < table.rowCount()) {
        value = table.valueAt(rowIndex, columnName, typeName, &error);
    } else {
        error = "PK_FETCH: current test invocation has no data row";
    }

    if (!value) {
        // fetchData 不带 file/line 参数，PK_FETCH 宏调用它时也没有传——
        // 位置信息只能退化成一个占位标记，不冒充调用点。
        PkTestCase::current().recordFailure("<PK_FETCH>", 0, error);
    }
    return value;
}

} // namespace PkTest
