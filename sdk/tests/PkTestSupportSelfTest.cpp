#include "PkTestSupportSelfTest.h"

// 三个测试函数各自钉住机器的一环。**刻意不 include 任何 pk 头**——凡本文件能
// 编过的东西，都是 PkTestCompatAll.h 被 `-include` 强制激活带来的（这正是
// libs/**/tests 里那一大批「靠 Qt 传递 include 才编得过」的测试所依赖的机制）。

void PkTestSupportSelfTest::testCompatMacroAliases()
{
    // QCOMPARE / QVERIFY / QVERIFY2 走 pk/test/compat/QTest 的别名——未做全量
    // sed 的真实测试源就是这种写法（pk/test/README.md §1 的 17 项 API 面）。
    // 字面量两侧都是 int，避开 PK_COMPARE 内部的 -Wsign-compare。
    QCOMPARE(1 + 1, 2);
    QVERIFY(true);
    QVERIFY2(2 > 1, "QVERIFY2 的消息参数必须能透传");
}

void PkTestSupportSelfTest::testForceIncludedScalarsAreActive()
{
    // qint32 是 pk/global 的标量族；qAbs 来自 pk/test/compat/QtGlobal（经
    // compat/QObject 传递）。两者本 TU 都没 include，能编过即证明聚合头
    // 被 -include 到了。
    qint32 value = -7;
    QCOMPARE(qAbs(value), 7);

    // ⚠ pkBound 返回 const T&（照抄 Qt 的 qBound 签名），实参是字面量时那个引用指
    // 向临时量，只在同一条 full-expression 内有效。PK_COMPARE 会把两个操作数拆到
    // 不同语句里求值，直接写 QCOMPARE(pkBound(0, 42, 10), 10) 读到的是悬垂值
    // （实测拿到 1）。pk/global/PkGlobal.h:150-157 明写「字面量调用先拷进具名变量」
    // ——这里照它做，顺带把这条语义钉在机器自测里。
    const int bounded = pkBound(0, 42, 10);
    QCOMPARE(bounded, 10);
}

void PkTestSupportSelfTest::testPreactivatedCompatTypesAreActive()
{
    // 只取 sizeof：证明类型完整可见（聚合头的 __has_include 预激活段生效），
    // 又不去赌各垫片成员函数的签名——那属于被测面，不属于机器。
    QVERIFY(sizeof(QRect) > 0);
    QVERIFY(sizeof(QString) > 0);
    QVERIFY(sizeof(QList<int>) > 0);

    // PkNamespace.h 的枚举族同样由聚合头预激活。
    Pk::Orientation orientation = Pk::Horizontal;
    QVERIFY(orientation == Pk::Horizontal);
}

SIMPLE_TEST_MAIN(PkTestSupportSelfTest)
