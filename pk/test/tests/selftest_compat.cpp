#include "generator_cases/compat_shape_case.h"
#include <QTest>
#include "selftest_util.h"

// PkTestBinder<CompatShapeCase> 由 pk_test_moc.py 生成，做法与 selftest_assert.cpp
// 相同：显式特化必须在 qExec<CompatShapeCase> 实例化前对本 TU 可见，所以
// #include 生成的 binder .cpp 而不是把它编成独立目标文件（见 Task 5 报告）。
#include "pk_binder_compat_shape_case.cpp"

// 走 Qt 拼法：QCOMPARE/QVERIFY 展开成 PK_COMPARE/PK_VERIFY，QTest 展开成 PkTest。
void CompatShapeCase::testPasses() { QCOMPARE(2 + 2, 4); QVERIFY(true); }
void CompatShapeCase::testFails()  { QCOMPARE(2 + 2, 5); }

void run_compat_selftests()
{
    CompatShapeCase tc;
    const char *argv[] = {"selftest"};
    const int rc = QTest::qExec(&tc, 1, const_cast<char **>(argv));
    SELF_EXPECT(rc == 1, "compat 路径下 Qt 拼法应当能编过并正确判定 1 个失败");
}
