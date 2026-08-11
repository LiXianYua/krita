#include "generator_cases/self_assert_case.h"
#include "selftest_util.h"

// PkTestBinder<SelfAssertCase> 特化由 pk_test_moc.py 生成（CMake 的
// pk_test_generate 触发构建），像 Qt moc 输出一样直接 #include 进本 TU——
// 显式特化必须在 qExec<SelfAssertCase> 实例化前对本 TU 可见，分开编译成
// 独立目标文件的话这里只看得到前置声明（不完整类型），编不过。
#include "pk_binder_self_assert_case.inc"

// 类声明在 generator_cases/self_assert_case.h（生成器的输入），
// 这里只放成员函数的定义与自测本体。

bool g_reachedAfterFailure = false;
bool g_qFailReturnedFalse = false;

void SelfAssertCase::passingVerify()  { PK_VERIFY(1 + 1 == 2); }
void SelfAssertCase::failingVerify()  { PK_VERIFY(1 + 1 == 3); }
void SelfAssertCase::unconditionalFail() { PK_FAIL("deliberate"); }
void SelfAssertCase::stopsAtFirstFailure() { PK_VERIFY(false); g_reachedAfterFailure = true; }

// QTest::qFail 的对应物：真实调用点（libs/pigment/tests/TestColorConversionSystem.cpp:85）
// 是从一个普通比较函数里直接调用的，不经 PK_FAIL/QFAIL 那套"从测试函数 return"
// 的宏协议——调用完还接着往下走。这里照这个用法调，不用 return。
void SelfAssertCase::callsQFailDirectly()
{
    g_qFailReturnedFalse = (PkTest::qFail("direct qFail call", __FILE__, __LINE__) == false);
}

void run_assert_selftests()
{
    SelfAssertCase tc;
    const char *argv[] = {"selftest"};
    int rc = PkTest::qExec(&tc, 1, const_cast<char **>(argv));

    // 5 个测试函数里 4 个应当失败（含新增的 callsQFailDirectly）→ qExec 返回失败
    // 函数个数（QTest 语义：非 0 即失败）
    SELF_EXPECT(rc == 4, "qExec 应返回失败的测试函数个数");
    SELF_EXPECT(!g_reachedAfterFailure, "PK_VERIFY 失败后必须 return，不得继续执行函数体");
    SELF_EXPECT(g_qFailReturnedFalse, "PkTest::qFail 应恒返回 false，与 Qt 的 QTest::qFail 一致");
}
