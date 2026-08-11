#pragma once

#include <any>
#include <cstdarg>
#include <string>
#include <typeinfo>
#include <vector>

// 一张数据表 = 若干列定义 + 若干行。QTest 里表挂在 QTestData 上，
// 我们同样用一个"当前表"的全局状态 —— _data() 函数体里的 addColumn/newRow
// 拿不到任何上下文对象，与断言宏同理。
class PkTestDataRow
{
public:
    explicit PkTestDataRow(class PkTestTable *table, std::string tag);

    template <typename T>
    PkTestDataRow &operator<<(const T &value)
    {
        appendValue(std::any(value), typeid(T).name());
        return *this;
    }

    const std::string &tag() const { return m_tag; }

private:
    void appendValue(std::any value, const char *typeName);

    class PkTestTable *m_table;
    std::string m_tag;
    std::size_t m_rowIndex;
};

class PkTestTable
{
public:
    void addColumn(const char *name, const char *typeName);
    PkTestDataRow &newRow(const char *tag);

    void clear();
    std::size_t rowCount() const { return m_rows.size(); }
    const std::string &tagAt(std::size_t i) const { return m_rows[i].tag; }

    // 返回列值指针；列名不存在或类型不符时返回 nullptr 并写 outError
    const std::any *valueAt(std::size_t rowIndex, const char *columnName,
                            const char *typeName, std::string *outError) const;

    static PkTestTable &current();

private:
    friend class PkTestDataRow;
    struct Column { std::string name; std::string typeName; };
    struct Row { std::string tag; std::vector<std::any> values; };

    std::vector<Column> m_columns;
    std::vector<Row> m_rows;
    std::vector<PkTestDataRow> m_rowHandles;
};

namespace PkTest {

template <typename T>
void addColumn(const char *name)
{
    PkTestTable::current().addColumn(name, typeid(T).name());
}

PkTestDataRow &newRow(const char *tag);
PkTestDataRow &addRow(const char *format, ...);

const char *currentDataTag();

// PK_FETCH 的取值入口。列不存在 / 类型不符时记一次失败并返回 nullptr——
// Qt 里 QFETCH 类型写错是运行时报错，不是 UB，我们对齐这一条。
const std::any *fetchData(const char *columnName, const char *typeName);

} // namespace PkTest

// QTest 的 QFETCH 展开成一个同名局部变量。类型不符时 Qt 是运行时报错，
// 我们对齐：fetchData 记失败并返回 nullptr，这里退化成值初始化的默认值，
// 让函数体能继续走到下一个断言（那个断言必然失败，错误信息不会丢）。
// 宏参数不能叫 name——预处理器是纯文本替换，会把宏体里 `typeid(Type).name()`
// 的那个 `.name()` 成员调用也当成参数名替换掉。
#define PK_FETCH(Type, pkFetchVarName)                                        \
    Type pkFetchVarName = [] {                                                \
        const std::any *pkFetched =                                          \
            PkTest::fetchData(#pkFetchVarName, typeid(Type).name());         \
        return pkFetched ? std::any_cast<Type>(*pkFetched) : Type();          \
    }()
