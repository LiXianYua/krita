#include "../PkTest.h"
#include "selftest_util.h"

static bool g_reachedAfterContinue = false;
static bool g_reachedAfterAbort = false;
// PK_SKIP 单独一个 case + 独立标记，不与上面的 expect-fail 计数混在一起——
// 混在一起会让 rc == 3 的失败函数计数断言失真。
static bool g_reachedAfterSkip = false;

class SelfExpectFailCase : public PkTestObject
{
    template <typename PkTestBinderArgT> friend struct PkTestBinder;
private:
    // 期望失败且确实失败 → XFAIL，不计入失败，Continue 模式继续跑
    void xfailContinue()
    {
        PK_EXPECT_FAIL("", "已知缺陷", PkTest::Continue);
        PK_VERIFY(false);
        g_reachedAfterContinue = true;
    }
    // 期望失败且确实失败 → XFAIL，Abort 模式立即 return
    void xfailAbort()
    {
        PK_EXPECT_FAIL("", "已知缺陷", PkTest::Abort);
        PK_VERIFY(false);
        g_reachedAfterAbort = true;
    }
    // 期望失败但通过了 → XPASS，必须计入失败
    void xpass()
    {
        PK_EXPECT_FAIL("", "以为会挂", PkTest::Continue);
        PK_VERIFY(true);
    }
    // 标记只作用于紧接着的一个断言，用完即清
    void markIsOneShot()
    {
        PK_EXPECT_FAIL("", "只管下一条", PkTest::Continue);
        PK_VERIFY(false);   // XFAIL
        PK_VERIFY(false);   // 这一条不再被豁免 → 真失败
    }
    void verify2Message() { PK_VERIFY2(1 == 2, "带解释的失败"); }
};

template <> struct PkTestBinder<SelfExpectFailCase> {
    static const char *className() { return "SelfExpectFailCase"; }
    static const PkTestFunction *functions() {
        static const PkTestFunction fns[] = {
            {"xfailContinue", [](PkTestObject *o){ static_cast<SelfExpectFailCase*>(o)->xfailContinue(); }, nullptr},
            {"xfailAbort",    [](PkTestObject *o){ static_cast<SelfExpectFailCase*>(o)->xfailAbort(); },    nullptr},
            {"xpass",         [](PkTestObject *o){ static_cast<SelfExpectFailCase*>(o)->xpass(); },         nullptr},
            {"markIsOneShot", [](PkTestObject *o){ static_cast<SelfExpectFailCase*>(o)->markIsOneShot(); }, nullptr},
            {"verify2Message",[](PkTestObject *o){ static_cast<SelfExpectFailCase*>(o)->verify2Message(); },nullptr},
        };
        return fns;
    }
    static int count() { return 5; }
    static const PkTestFunction *initTestCase()     { return nullptr; }
    static const PkTestFunction *cleanupTestCase()  { return nullptr; }
    static const PkTestFunction *initFn()           { return nullptr; }
    static const PkTestFunction *cleanupFn()        { return nullptr; }
    static const PkTestFunction *initTestCaseData() { return nullptr; }
};

class SelfSkipCase : public PkTestObject
{
    template <typename PkTestBinderArgT> friend struct PkTestBinder;
private:
    void skipped()
    {
        PK_SKIP("环境不具备");
        g_reachedAfterSkip = true;   // 不应到达
    }
};

template <> struct PkTestBinder<SelfSkipCase> {
    static const char *className() { return "SelfSkipCase"; }
    static const PkTestFunction *functions() {
        static const PkTestFunction fns[] = {
            {"skipped", [](PkTestObject *o){ static_cast<SelfSkipCase*>(o)->skipped(); }, nullptr},
        };
        return fns;
    }
    static int count() { return 1; }
    static const PkTestFunction *initTestCase()     { return nullptr; }
    static const PkTestFunction *cleanupTestCase()  { return nullptr; }
    static const PkTestFunction *initFn()           { return nullptr; }
    static const PkTestFunction *cleanupFn()        { return nullptr; }
    static const PkTestFunction *initTestCaseData() { return nullptr; }
};

void run_expectfail_selftests()
{
    SelfExpectFailCase tc;
    const char *argv[] = {"selftest"};
    const int rc = PkTest::qExec(&tc, 1, const_cast<char **>(argv));

    SELF_EXPECT(g_reachedAfterContinue,
                "PkTest::Continue 模式下 XFAIL 之后必须继续执行函数体");
    SELF_EXPECT(!g_reachedAfterAbort,
                "PkTest::Abort 模式下 XFAIL 之后必须 return");
    // xpass / markIsOneShot / verify2Message 三个应当失败；
    // xfailContinue / xfailAbort 两个是 XFAIL，不计入失败
    SELF_EXPECT(rc == 3,
                "XFAIL 不计入失败，XPASS 计入失败：应恰有 3 个失败的测试函数");

    SelfSkipCase sc;
    const int skipRc = PkTest::qExec(&sc, 1, const_cast<char **>(argv));
    SELF_EXPECT(skipRc == 0, "PK_SKIP 不计入失败（qExec 返回失败函数个数，跳过的不算）");
    SELF_EXPECT(!g_reachedAfterSkip, "PK_SKIP 之后必须 return，不得继续执行函数体");
}
