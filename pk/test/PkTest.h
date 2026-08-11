#pragma once

#include "PkTestObject.h"
#include "PkTestCase.h"
#include "PkTestCompare.h"

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
        B::initTestCase(), B::cleanupTestCase(),
        B::initFn(), B::cleanupFn(), B::initTestCaseData()
    };
    return execPlan(obj, plan, argc, argv);
}

} // namespace PkTest

// ---- 断言宏 ----
//
// QTest 语义：断言失败时**从当前测试函数 return**（不是 abort、不是抛异常）。
// 所以这些宏只能用在返回 void 的测试函数体内 —— 与 QTest 完全一致。

#define PK_VERIFY(statement)                                          \
    do {                                                              \
        if (!static_cast<bool>(statement)) {                          \
            PkTestCase::current().recordFailure(                      \
                __FILE__, __LINE__, "'" #statement "' returned FALSE"); \
            return;                                                   \
        }                                                             \
    } while (false)

#define PK_FAIL(message)                                              \
    do {                                                              \
        PkTestCase::current().recordFailure(__FILE__, __LINE__, (message)); \
        return;                                                       \
    } while (false)

#define PK_COMPARE(actual, expected)                                        \
    do {                                                                    \
        if (!pkTestCompare((actual), (expected))) {                         \
            PkTestCase::current().recordFailure(                            \
                __FILE__, __LINE__,                                         \
                pkTestCompareFailureMessage(#actual, #expected,             \
                                            pkTestToString(actual),         \
                                            pkTestToString(expected)));     \
            return;                                                         \
        }                                                                   \
    } while (false)
