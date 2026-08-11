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

// qExec 的类型无关部分。模板 qExec 只负责把 PkTestBinder<T> 的静态信息
// 拆成普通指针传进来 —— 这样 runner 的实现全在 .cpp 里，不随每个测试类实例化一遍。
//
// 全局作用域而非 namespace PkTest：它是纯内部管道类型，不是「QTest::addColumn
// 这一族本来就是 namespace 里的函数」那个例外，类名照全局 Pk 前缀的规则走。
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

namespace PkTest {

int execPlan(PkTestObject *obj, const PkTestPlan &plan, int argc, char **argv);

// QTest::qFail 的对应物——QFAIL/QVERIFY2 等宏在 Qt 里就是靠它记失败，也是
// 唯二两个被真实调用点**直接调用**（不经宏）的 QTest:: 成员之一（另一个是
// qCompare，R-11 判定排除，见 pk/test/README.md §2）。message 为 nullptr 时
// 按空字符串处理，不做额外校验——与 recordFailure 本身的容错口径一致。
bool qFail(const char *message, const char *file, int line);

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

// 两侧各**只求值一次**，与 QCOMPARE 一致（Qt 把实参按引用传给 qCompare）。
// 先绑到 const auto & 局部量再比较与字符串化——直接在失败分支里重新写
// `pkTestToString(actual)` 会让带副作用的表达式（`PK_COMPARE(list.takeFirst(), x)`）
// 一失败就多推进一次状态。变量名带 pkCompare 前缀避免与用户表达式撞名
// （PK_FETCH 的 pkFetchVarName 踩过同一个坑）。
#define PK_COMPARE(actual, expected)                                        \
    do {                                                                    \
        const auto &pkCompareActual_ = (actual);                            \
        const auto &pkCompareExpected_ = (expected);                        \
        const bool pkCompareOk_ =                                           \
            pkTestCompare(pkCompareActual_, pkCompareExpected_);            \
        if (PkTestCase::current().checkResult(                             \
                pkCompareOk_, __FILE__, __LINE__,                          \
                pkCompareOk_ ? std::string()                               \
                             : pkTestCompareFailureMessage(#actual, #expected, \
                                            pkTestToString(pkCompareActual_), \
                                            pkTestToString(pkCompareExpected_)))) { \
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

// mode 是**拼接**出 `PkTest::mode`，不是原样透传——Qt 的 QEXPECT_FAIL 宏体里写的是
// `QTest::mode`，所以真实调用点写的是裸 `Continue` / `Abort`（实测 Krita 全仓
// 零处写 `QTest::Continue`）。原样透传会让每一个真实调用点报
// "'Continue' was not declared in this scope"，而改名 sed 与 `#define QTest PkTest`
// 都救不了——调用点根本没写命名空间。
#define PK_EXPECT_FAIL(dataIndex, comment, mode)                       \
    PkTestCase::current().expectFail((dataIndex), (comment), PkTest::mode, __FILE__, __LINE__)

// ---- PK_TEST_MAIN 家族 ----
//
// Qt 里 QTEST_MAIN/QTEST_APPLESS_MAIN/QTEST_GUILESS_MAIN 的区别只是创建
// QApplication / 什么都不创建 / 创建 QCoreApplication。零 Qt 之后没有 app
// 对象可创建，三者退化成完全相同的代码。仍然保留三个名字：D-23 的全量 sed
// 是一对一改名，合并成一个宏会让调用点需要人工判断改成哪个。

#define PK_TEST_MAIN(TestObject)                        \
    int main(int argc, char *argv[])                    \
    {                                                   \
        TestObject tc;                                  \
        return PkTest::qExec(&tc, argc, argv);          \
    }

#define PK_TEST_APPLESS_MAIN(TestObject) PK_TEST_MAIN(TestObject)
#define PK_TEST_GUILESS_MAIN(TestObject) PK_TEST_MAIN(TestObject)
