#include "test_stream.h"
#include "../PkDebug.h"
#include "../PkLogSink.h"
#include <string>
#include <vector>

static std::vector<std::string> g_lines;
static void capture(PkLogLevel, const PkLogContext &, const char *m, void *)
{ g_lines.push_back(m); }

// 每个用例都在自己的作用域里让 PkDebug 析构，flush 才发生。
static std::string emitted(void (*body)(PkDebug))
{
    g_lines.clear();
    const int h = PkLogAddSink(capture, nullptr);
    body(PkDebugMakeForTest(PkLogDebug, "krita.general"));
    PkLogRemoveSink(h);
    return g_lines.empty() ? std::string("<none>") : g_lines[0];
}

void PkDebugStreamTest::testDefaultInsertsSpacesBetweenItemsButNotAtEnd()
{
    PK_COMPARE(emitted([](PkDebug d){ d << 1 << 2 << 3; }), std::string("1 2 3"));
}

void PkDebugStreamTest::testNospaceSuppressesSeparator()
{
    PK_COMPARE(emitted([](PkDebug d){ d.nospace() << 1 << 2 << 3; }), std::string("123"));
}

// 实测 1c/3b：const char* 与 char **从来不加引号**，noquote 前后一样。
void PkDebugStreamTest::testCharPointerAndCharAreNeverQuoted()
{
    PK_COMPARE(emitted([](PkDebug d){ d << "ab" << 'c'; }),           std::string("ab c"));
    PK_COMPARE(emitted([](PkDebug d){ d.noquote() << "ab" << 'c'; }), std::string("ab c"));
}

// 实测 1e：空 const char* 打空串，不是 "(null)"。
void PkDebugStreamTest::testNullCharPointerPrintsEmpty()
{
    PK_COMPARE(emitted([](PkDebug d){ d << (const char *)nullptr; }), std::string(""));
}

// 实测 1d/1f：bool → true/false；float 2.0f → "2"。
void PkDebugStreamTest::testBoolAndFloatFormatting()
{
    PK_COMPARE(emitted([](PkDebug d){ d << true << false; }), std::string("true false"));
    PK_COMPARE(emitted([](PkDebug d){ d << 1.5 << 2.0f; }),   std::string("1.5 2"));
}

// 实测 2d：space() 立即写空格 —— nospace 段与 space 段之间那个空格由 space() 补出。
void PkDebugStreamTest::testSpaceEagerlyEmitsSeparator()
{
    PK_COMPARE(emitted([](PkDebug d){ d.nospace() << 1 << 2; d.space() << 3 << 4; }),
               std::string("12 3 4"));
}

// 实测 12a：maybeSpace() 在 nospace 状态下什么都不写。
void PkDebugStreamTest::testMaybeSpaceRespectsFlag()
{
    PK_COMPARE(emitted([](PkDebug d){ d.nospace() << "a"; d.maybeSpace() << "b"; }),
               std::string("ab"));
}

// 96 处用户重载的形态：按值收、改模式、按值还。必须只 flush 一行。
struct Foo { int x = 5; };
static PkDebug operator<<(PkDebug dbg, const Foo &f)
{
    dbg.nospace() << "Foo(" << f.x << ")";
    return dbg.space();
}

void PkDebugStreamTest::testUserOperatorPatternRoundTrips()
{
    PK_COMPARE(emitted([](PkDebug d){ d << Foo{} << 9; }), std::string("Foo(5) 9"));
}

void PkDebugStreamTest::testCopiesShareOneLineAndFlushOnce()
{
    g_lines.clear();
    const int h = PkLogAddSink(capture, nullptr);
    { PkDebug d = PkDebugMakeForTest(PkLogDebug, "krita.general"); PkDebug e = d; e << 1; }
    PkLogRemoveSink(h);
    PK_COMPARE(static_cast<int>(g_lines.size()), 1);
}

struct Opaque { int a; };
void PkDebugStreamTest::testUnprintableTypeDoesNotBreakCompile()
{
    PK_COMPARE(emitted([](PkDebug d){ d << Opaque{1}; }), std::string("<unprintable>"));
}

// PkString 是 QString 的对应物，所以它走 **QString 的引号规则**（实测 1b/3a）：
// 默认带引号，noquote() 后不带。这是全套里唯一受 quote 影响的一类。
struct FakePkString { std::string PkToUtf8() const { return "hi"; } };
void PkDebugStreamTest::testDuckTypedPkToUtf8IsQuotedLikeQString()
{
    PK_COMPARE(emitted([](PkDebug d){ d << FakePkString{} << FakePkString{}; }),
               std::string("\"hi\" \"hi\""));
    PK_COMPARE(emitted([](PkDebug d){ d.noquote() << FakePkString{} << FakePkString{}; }),
               std::string("hi hi"));
    // 实测 3d：中途切回 quote
    PK_COMPARE(emitted([](PkDebug d){ d.noquote() << FakePkString{}; d.quote() << FakePkString{}; }),
               std::string("hi \"hi\""));
}

// 实测 7a/7e：一行内粘住，不跨行。
void PkDebugStreamTest::testFieldWidthIsStickyWithinLineOnly()
{
    PK_COMPARE(emitted([](PkDebug d){ d << qSetFieldWidth(6) << 1 << 2; }),
               std::string("     1           2"));   // 尾随空格在 flush 时去掉
    PK_COMPARE(emitted([](PkDebug d){ d << 1 << 2; }), std::string("1 2"));
}

// 禁用路径：flush 时什么都不产出——sink 完全收不到消息（不是收到空串）。
void PkDebugStreamTest::testSilentDebugProducesNothing()
{
    g_lines.clear();
    const int h = PkLogAddSink(capture, nullptr);
    { PkDebug d = PkDebugMakeSilent(); d << "should" << "not" << "appear"; }
    PkLogRemoveSink(h);
    PK_COMPARE(static_cast<int>(g_lines.size()), 0);
}

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_stream.inc"

int run_stream_tests(int argc, char **argv)
{
    PkDebugStreamTest tc;
    return PkTest::qExec(&tc, argc, argv);
}
