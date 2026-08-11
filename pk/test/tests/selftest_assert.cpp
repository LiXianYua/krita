#include "../PkTest.h"
#include "selftest_util.h"

// 全局标记必须先于类声明可见：内联成员函数体虽按"完整类之后"延迟处理，
// 那只让本类的后续成员对前面的成员可见，不会让文件里更晚出现的全局变量
// 提前可见——放在类后面会是一处真正的编译错误，不是风格问题。
bool g_reachedAfterFailure = false;

// 一个手写的"测试类"，形状与 Krita 真实测试类一致，但 binder 手写而非生成。
// Task 5 上生成器之后，这个手写 binder 仍然保留 —— 它是生成器的对照组。
class SelfAssertCase : public PkTestObject
{
    template <typename PkTestBinderArgT> friend struct PkTestBinder;
private:
    void passingVerify()  { PK_VERIFY(1 + 1 == 2); }
    void failingVerify()  { PK_VERIFY(1 + 1 == 3); }
    void unconditionalFail() { PK_FAIL("deliberate"); }
    void stopsAtFirstFailure() { PK_VERIFY(false); g_reachedAfterFailure = true; }
};

template <> struct PkTestBinder<SelfAssertCase> {
    static const char *className() { return "SelfAssertCase"; }
    static const PkTestFunction *functions() {
        static const PkTestFunction fns[] = {
            {"passingVerify",      [](PkTestObject *o){ static_cast<SelfAssertCase*>(o)->passingVerify(); },      nullptr},
            {"failingVerify",      [](PkTestObject *o){ static_cast<SelfAssertCase*>(o)->failingVerify(); },      nullptr},
            {"unconditionalFail",  [](PkTestObject *o){ static_cast<SelfAssertCase*>(o)->unconditionalFail(); },  nullptr},
            {"stopsAtFirstFailure",[](PkTestObject *o){ static_cast<SelfAssertCase*>(o)->stopsAtFirstFailure(); },nullptr},
        };
        return fns;
    }
    static int count() { return 4; }
    static const PkTestFunction *initTestCase()     { return nullptr; }
    static const PkTestFunction *cleanupTestCase()  { return nullptr; }
    static const PkTestFunction *initFn()           { return nullptr; }
    static const PkTestFunction *cleanupFn()        { return nullptr; }
    static const PkTestFunction *initTestCaseData() { return nullptr; }
};

void run_assert_selftests()
{
    SelfAssertCase tc;
    const char *argv[] = {"selftest"};
    int rc = PkTest::qExec(&tc, 1, const_cast<char **>(argv));

    // 4 个测试函数里 3 个应当失败 → qExec 返回失败函数个数（QTest 语义：非 0 即失败）
    SELF_EXPECT(rc == 3, "qExec 应返回失败的测试函数个数");
    SELF_EXPECT(!g_reachedAfterFailure, "PK_VERIFY 失败后必须 return，不得继续执行函数体");
}
