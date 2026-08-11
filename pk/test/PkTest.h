#pragma once

#include "PkTestObject.h"
#include "PkTestCase.h"
#include "PkTestCompare.h"
#include "PkTestData.h"

// 每个测试类由生成器（pk_test_moc.py）特化一个 PkTestBinder。
// 只前置声明：Q_OBJECT 展开成对它的 friend 声明，所以它必须先可见。
template <typename T> struct PkTestBinder;

struct PkTestFunction
{
    const char *name;
    void (*invoke)(PkTestObject *);
    // 伴生数据函数名（"testFoo_data"），没有则为 nullptr。
    const char *dataName;
};

namespace PkTest {

// qExec 的类型无关部分。模板 qExec 只负责把 PkTestBinder<T> 的静态信息
// 拆成普通指针传进来 —— 这样 runner 的实现全在 .cpp 里，不随每个测试类实例化一遍。
struct PkTestPlan
{
    const char *className;
    const PkTestFunction *functions;
    int count;
    // 数据函数（"xxx_data"）单独一张表，runner 按 PkTestFunction::dataName 查。
    const PkTestFunction *dataFunctions;
    int dataCount;
    const PkTestFunction *initTestCase;
    const PkTestFunction *cleanupTestCase;
    const PkTestFunction *initFn;
    const PkTestFunction *cleanupFn;
    const PkTestFunction *initTestCaseData;
};

int execPlan(PkTestObject *obj, const PkTestPlan &plan, int argc, char **argv);

template <typename T>
int qExec(T *obj, int argc = 0, char **argv = nullptr)
{
    using B = PkTestBinder<T>;
    const PkTestPlan plan{
        B::className(), B::functions(), B::count(),
        B::dataFunctions(), B::dataCount(),
        B::initTestCase(), B::cleanupTestCase(),
        B::initFn(), B::cleanupFn(), B::initTestCaseData()
    };
    return execPlan(obj, plan, argc, argv);
}

// QTest::Continue / QTest::Abort 的对应物。放在 namespace 里而非全局：
// Continue / Abort 是极易撞名的标识符。
constexpr PkTestFailMode Continue = PkTestContinueMode;
constexpr PkTestFailMode Abort    = PkTestAbortMode;

} // namespace PkTest

// ---- 断言宏 ----
//
// QTest 语义：断言失败时**从当前测试函数 return**（不是 abort、不是抛异常）。
// 所以这些宏只能用在返回 void 的测试函数体内 —— 与 QTest 完全一致。
//
// 全部经 PkTestCase::checkResult() 统一入口，而不是各自直接 recordFailure——
// 否则 PK_EXPECT_FAIL 只能豁免它认识的宏，漏掉的宏永远不会被判成 XFAIL/XPASS。

#define PK_VERIFY(statement)                                          \
    do {                                                              \
        if (PkTestCase::current().checkResult(                       \
                static_cast<bool>(statement), __FILE__, __LINE__,     \
                "'" #statement "' returned FALSE")) {                 \
            return;                                                   \
        }                                                             \
    } while (false)

#define PK_FAIL(message)                                              \
    do {                                                              \
        if (PkTestCase::current().checkResult(                       \
                false, __FILE__, __LINE__, (message))) {              \
            return;                                                   \
        }                                                             \
    } while (false)

#define PK_COMPARE(actual, expected)                                        \
    do {                                                                    \
        const bool pkCompareOk_ = pkTestCompare((actual), (expected));      \
        if (PkTestCase::current().checkResult(                             \
                pkCompareOk_, __FILE__, __LINE__,                          \
                pkCompareOk_ ? std::string()                               \
                             : pkTestCompareFailureMessage(#actual, #expected, \
                                            pkTestToString(actual),         \
                                            pkTestToString(expected)))) {   \
            return;                                                         \
        }                                                                   \
    } while (false)

#define PK_VERIFY2(statement, description)                            \
    do {                                                              \
        if (PkTestCase::current().checkResult(                        \
                static_cast<bool>(statement), __FILE__, __LINE__,     \
                std::string("'" #statement "' returned FALSE (")      \
                    + (description) + ")")) {                         \
            return;                                                   \
        }                                                             \
    } while (false)

#define PK_SKIP(description)                                          \
    do {                                                              \
        PkTestCase::current().skipCurrent((description), __FILE__, __LINE__); \
        return;                                                       \
    } while (false)

#define PK_EXPECT_FAIL(dataIndex, comment, mode)                      \
    PkTestCase::current().expectFail((dataIndex), (comment), (mode), __FILE__, __LINE__)
