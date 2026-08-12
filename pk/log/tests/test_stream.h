#pragma once
#include <QObject>      // → pk/test/compat/QObject，提供 QObject/Q_OBJECT/Q_SLOTS
#include <PkTest.h>

class PkDebugStreamTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testDefaultInsertsSpacesBetweenItemsButNotAtEnd();
    void testNospaceSuppressesSeparator();
    void testCharPointerAndCharAreNeverQuoted();
    void testNullCharPointerPrintsEmpty();
    void testBoolAndFloatFormatting();
    void testSpaceEagerlyEmitsSeparator();
    void testMaybeSpaceRespectsFlag();
    void testCopiesShareOneLineAndFlushOnce();
    void testUserOperatorPatternRoundTrips();
    void testUnprintableTypeDoesNotBreakCompile();
    void testDuckTypedPkToUtf8IsQuotedLikeQString();
    void testFieldWidthIsStickyWithinLineOnly();
    // Produces 里明确要求："PkDebugMakeSilent() ... 测试要能验证禁用时不产出"，
    // 但 brief Step1 给的用例列表漏了这条——补上，不能算 Task2 没做完。
    void testSilentDebugProducesNothing();
};
