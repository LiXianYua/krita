#pragma once

#include <any>
#include <cstdarg>
#include <deque>
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
        // 类型身份只取 std::any 自己 decay 之后的 value.type()（appendValue 内部
        // 用它比对列类型）——不再额外传 typeid(T).name()：T 是模板推导出来的
        // 原始类型（数组字面量推出来是 char[N]，未 decay），而 std::any(value)
        // 内部会把它 decay 成指针再存。两个身份不一致会导致同一个值在这里通过
        // 校验、valueAt() 取值时却因类型名对不上而被判定为不同类型。
        appendValue(std::any(value));
        return *this;
    }

    // 非模板重载（Qt qtestdata.h:81-86 同款）：字符串字面量自动转 PkString 再存。
    // 重载决议：非模板优先于模板，const char* 参数（含数组→指针 decay）统一走
    // 这条重载，不再走上面的模板推导 const char[N]。
    friend PkTestDataRow &operator<<(PkTestDataRow &row, const char *value);

private:
    void appendValue(std::any value);

    class PkTestTable *m_table;
    std::string m_tag;
    std::size_t m_rowIndex;
};

// 声明在类外，与 Qt 的 qtestdata.h 风格一致（operator<< 是自由函数）。
PkTestDataRow &operator<<(PkTestDataRow &row, const char *value);

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
    // deque 而非 vector：newRow 返回 m_rowHandles.back() 的引用，vector 扩容会
    // 搬走元素、让上一行拿到的引用失效。deque 的 push_back 不使旧元素引用失效，
    // 于是"引用只在同一条链式语句里用完"从一条约定变成一条机制保证。
    std::deque<PkTestDataRow> m_rowHandles;
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
// 默认值不能直接写 `Type()`（或 `Type{}`）：Type 可以是指针类型（如
// `const char*`），两者都要求类型名是单个 simple-type-specifier，指针
// 声明符不满足——`Type()` 会被解析成函数类型声明而报语法错，`Type{}`
// 则只能靠非标准的 GNU 复合字面量扩展才编得过。改成先声明一个具名局部
// 变量 `Type pkFetchDefault{};` 再在三目表达式里引用它：声明语句里的
// 花括号初始化对任意类型（含指针）都是标准语法，没有这条限制。
#define PK_FETCH(Type, pkFetchVarName)                                        \
    Type pkFetchVarName = [] {                                                \
        const std::any *pkFetched =                                          \
            PkTest::fetchData(#pkFetchVarName, typeid(Type).name());         \
        Type pkFetchDefault{};                                               \
        return pkFetched ? std::any_cast<Type>(*pkFetched) : pkFetchDefault; \
    }()
