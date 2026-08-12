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
    // 评审 Important 项：flush 的尾随空格砍法——nospace 时（space 标志为假）
    // 一个都不砍；space 时（标志为真）无论字面尾随空格多少，只砍一个。
    void testNospaceKeepsLiteralTrailingSpaces();
    void testSpaceChopsExactlyOneTrailingSpace();
    // Produces 里明确要求："PkDebugMakeSilent() ... 测试要能验证禁用时不产出"，
    // 但 brief Step1 给的用例列表漏了这条——补上，不能算 Task2 没做完。
    void testSilentDebugProducesNothing();
};
