#include "../PkTest.h"
#include "selftest_util.h"

// runner 与 PkTestCase 状态机层面的自测：命令行过滤、initTestCase 里的 PK_SKIP、
// 以及"没有活动函数上下文时记失败"这条契约。断言层的自测在 selftest_assert.cpp。

static int g_alphaRuns = 0;
static int g_betaRuns = 0;

class SelfFilterCase : public PkTestObject
{
    template <typename PkTestBinderArgT> friend struct PkTestBinder;
private:
    void alpha() { ++g_alphaRuns; }
    void beta()  { ++g_betaRuns; }
};

template <> struct PkTestBinder<SelfFilterCase> {
    static const char *className() { return "SelfFilterCase"; }
    static const PkTestFunction *functions() {
        static const PkTestFunction fns[] = {
            {"alpha", [](PkTestObject *o){ static_cast<SelfFilterCase*>(o)->alpha(); }, nullptr},
            {"beta",  [](PkTestObject *o){ static_cast<SelfFilterCase*>(o)->beta(); },  nullptr},
        };
        return fns;
    }
    static int count() { return 2; }
    static const PkTestFunction *dataFunctions() { return nullptr; }
    static int dataCount() { return 0; }
    static const PkTestFunction *initTestCase()     { return nullptr; }
    static const PkTestFunction *cleanupTestCase()  { return nullptr; }
    static const PkTestFunction *initFn()           { return nullptr; }
    static const PkTestFunction *cleanupFn()        { return nullptr; }
    static const PkTestFunction *initTestCaseData() { return nullptr; }
};

// initTestCase 里的 PK_SKIP 跳过的是整个测试类，不只是 initTestCase 自己。
static int g_afterSkippedInitRuns = 0;

class SelfSkipInInitCase : public PkTestObject
{
    template <typename PkTestBinderArgT> friend struct PkTestBinder;
private:
    void initTestCase() { PK_SKIP("整个测试类都不具备运行条件"); }
    void shouldNotRun() { ++g_afterSkippedInitRuns; }
};

template <> struct PkTestBinder<SelfSkipInInitCase> {
    static const char *className() { return "SelfSkipInInitCase"; }
    static const PkTestFunction *functions() {
        static const PkTestFunction fns[] = {
            {"shouldNotRun", [](PkTestObject *o){ static_cast<SelfSkipInInitCase*>(o)->shouldNotRun(); }, nullptr},
        };
        return fns;
    }
    static int count() { return 1; }
    static const PkTestFunction *dataFunctions() { return nullptr; }
    static int dataCount() { return 0; }
    static const PkTestFunction *initTestCase() {
        static const PkTestFunction fn{"initTestCase",
            [](PkTestObject *o){ static_cast<SelfSkipInInitCase*>(o)->initTestCase(); }, nullptr};
        return &fn;
    }
    static const PkTestFunction *cleanupTestCase()  { return nullptr; }
    static const PkTestFunction *initFn()           { return nullptr; }
    static const PkTestFunction *cleanupFn()        { return nullptr; }
    static const PkTestFunction *initTestCaseData() { return nullptr; }
};

void run_runner_selftests()
{
    // ---- 未知的测试函数名必须报错，不能静默跑 0 个测试再退出 0
    {
        SelfFilterCase tc;
        g_alphaRuns = g_betaRuns = 0;
        const char *argv[] = {"selftest", "thisFunctionDoesNotExist"};
        const int rc = PkTest::qExec(&tc, 2, const_cast<char **>(argv));
        SELF_EXPECT(rc != 0,
                    "过滤器里的函数名不存在时必须非零退出（Qt: Unknown testfunction），"
                    "不能静默跑 0 个测试再报全绿");
        SELF_EXPECT(g_alphaRuns == 0 && g_betaRuns == 0,
                    "名字不存在时一个测试函数都不该跑");
    }

    // ---- 存在的名字照常只跑那一个
    {
        SelfFilterCase tc;
        g_alphaRuns = g_betaRuns = 0;
        const char *argv[] = {"selftest", "alpha"};
        const int rc = PkTest::qExec(&tc, 2, const_cast<char **>(argv));
        SELF_EXPECT(rc == 0, "按存在的函数名过滤应当正常跑完");
        SELF_EXPECT(g_alphaRuns == 1 && g_betaRuns == 0, "只跑被点名的那一个测试函数");
    }

    // ---- 带值开关的值不能被当成函数名过滤器
    {
        SelfFilterCase tc;
        g_alphaRuns = g_betaRuns = 0;
        const char *argv[] = {"selftest", "-o", "results.xml"};
        const int rc = PkTest::qExec(&tc, 3, const_cast<char **>(argv));
        SELF_EXPECT(rc == 0,
                    "-o 的值不是测试函数名，不能被当成过滤器（那样会退化成 Unknown testfunction）");
        SELF_EXPECT(g_alphaRuns == 1 && g_betaRuns == 1,
                    "没有裸参数就是不过滤：两个测试函数都该跑");
    }

    // ---- PK_SKIP 写在 initTestCase 里 → 整个测试类都不跑
    {
        SelfSkipInInitCase tc;
        g_afterSkippedInitRuns = 0;
        const char *argv[] = {"selftest"};
        const int rc = PkTest::qExec(&tc, 1, const_cast<char **>(argv));
        SELF_EXPECT(g_afterSkippedInitRuns == 0,
                    "initTestCase 里 PK_SKIP 之后，后续测试函数一个都不该跑");
        SELF_EXPECT(rc == 0, "跳过不计入失败");
    }

    // ---- 没有活动函数上下文时记失败必须响亮：不能被静默记进残留上下文
    {
        PkTestCase &state = PkTestCase::current();
        state.beginRun();
        const int before = state.failedFunctionCount();
        state.recordFailure("<selftest>", 0, "记在任何测试函数之外的失败");
        SELF_EXPECT(state.failedFunctionCount() == before + 1,
                    "没有活动函数上下文时 recordFailure 必须自己计一次失败，"
                    "否则它会被下一次 beginFunction 抹掉（_data() 吞断言那条 bug 的成因）");
    }
}
