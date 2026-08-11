#include "PkTestData.h"
#include "PkTestCase.h"
#include <cstdio>
#include <cstring>

PkTestDataRow::PkTestDataRow(PkTestTable *table, std::string tag)
    : m_table(table), m_tag(std::move(tag)), m_rowIndex(table->m_rows.size())
{
    table->m_rows.push_back(PkTestTable::Row{m_tag, {}});
}

void PkTestDataRow::appendValue(std::any value, const char *typeName)
{
    // 类型信息已经在 std::any 里带着（typeid(T)），append 阶段不用再存一遍——
    // 真正的类型校验发生在 valueAt()：拿运行时的 any.type() 和 PK_FETCH 要的类型比。
    (void)typeName;
    m_table->m_rows[m_rowIndex].values.push_back(std::move(value));
}

void PkTestTable::addColumn(const char *name, const char *typeName)
{
    m_columns.push_back(Column{name ? name : "", typeName ? typeName : ""});
}

PkTestDataRow &PkTestTable::newRow(const char *tag)
{
    // 存进 m_rowHandles 只为了让返回的引用活过这一条语句（`newRow(...) << a << b;`）。
    // 下一次 newRow/addRow 的 push_back 可能重新分配、搬走这个对象，但那时上一行的
    // 引用早已用完，不存在悬空访问。
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
    return PkTestCase::current().currentDataTag().c_str();
}

const std::any *fetchData(const char *columnName, const char *typeName)
{
    const std::string &tag = PkTestCase::current().currentDataTag();
    const PkTestTable &table = PkTestTable::current();

    std::size_t rowIndex = table.rowCount();
    for (std::size_t i = 0; i < table.rowCount(); ++i) {
        if (table.tagAt(i) == tag) {
            rowIndex = i;
            break;
        }
    }

    std::string error;
    const std::any *value = nullptr;
    if (rowIndex < table.rowCount()) {
        value = table.valueAt(rowIndex, columnName, typeName, &error);
    } else {
        error = "PK_FETCH: current test invocation has no data row";
    }

    if (!value) {
        // fetchData 的签名（照抄计划）不带 file/line，PK_FETCH 宏也没有传——
        // 位置信息只能退化成一个占位标记，不冒充调用点。
        PkTestCase::current().recordFailure("<PK_FETCH>", 0, error);
    }
    return value;
}

} // namespace PkTest
