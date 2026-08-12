#include "test_compat.h"

// 顶部真的写 Qt 拼法——走 -I pk/log/compat 解析到我们的实现，不是拿 Pk 名字
// 抄一遍同样的测试（brief 的硬要求：这才证明垫片"一个字都不改"真的能编过）。
#include <QDebug>
#include <QLoggingCategory>

#include "../PkLogSink.h"

#include <string>
#include <vector>

namespace {

std::vector<std::string> g_lines;
void capture(PkLogLevel, const PkLogContext &, const char *message, void *)
{
    g_lines.push_back(message);
}

} // namespace

// Step 1 场景原样。_test41000 刻意用真实 kis_debug.h 里 "krita.general" 分类
// 函数的真名 _41000 同款命名法（这里加 test 前缀避免撞 Task 5 真正接进来的
// 那个符号），提前对齐将来的真实调用点。
Q_LOGGING_CATEGORY(_test41000, "krita.general", QtInfoMsg)

// 与 kis_debug.h 同形的宏：dbgKrita/warnKrita 的缩影。
#define dbgTestKrita  qCDebug(_test41000)
#define warnTestKrita qCWarning(_test41000)

// 96 处用户重载的真实形态。用 QDebug（→ 垫片展开成 PkDebug）而不是直接写
// PkDebug——这一条才真的验证"一个字都不改"能编过，不是 Task 2 那条用 PkDebug
// 直写的 Foo 测试的重复。
struct Bar { int v = 3; };
QDebug operator<<(QDebug dbg, const Bar &b)
{
    dbg.nospace() << "Bar(" << b.v << ")";
    return dbg.space();
}

// 三参形态的默认级别 QtInfoMsg：debug 禁用、warning 启用
// （PkLoggingCategory.h 判据①，QtInfoMsg 经 compat/QDebug 按名字映射到
// PkLogInfo，不是按真 Qt 的数值 4）。
void PkLogCompatTest::testCategoryGatesDebugButNotWarning()
{
    g_lines.clear();
    const int h = PkLogAddSink(capture, nullptr);
    dbgTestKrita << Bar{};
    warnTestKrita << Bar{};
    PkLogRemoveSink(h);
    PK_COMPARE(static_cast<int>(g_lines.size()), 1);
    PK_COMPARE(g_lines[0], std::string("Bar(3)"));
}

void PkLogCompatTest::testUserOperatorOverloadOnQDebugRoundTrips()
{
    g_lines.clear();
    const int h = PkLogAddSink(capture, nullptr);
    warnTestKrita << Bar{} << 9;
    PkLogRemoveSink(h);
    PK_COMPARE(static_cast<int>(g_lines.size()), 1);
    PK_COMPARE(g_lines[0], std::string("Bar(3) 9"));
}

// qDebug/qWarning 无分类版本，流式形态：`qWarning() << ...`。
// const char* 从不加引号（Task 2 实测事实），所以是 "hello 42" 不是 "\"hello\" 42"。
void PkLogCompatTest::testFreeFunctionStreamFormWorks()
{
    g_lines.clear();
    const int h = PkLogAddSink(capture, nullptr);
    qWarning() << "hello" << 42;
    PkLogRemoveSink(h);
    PK_COMPARE(static_cast<int>(g_lines.size()), 1);
    PK_COMPARE(g_lines[0], std::string("hello 42"));
}

// printf 变参形态：`qDebug("fmt", ...)`——kis_debug.h 里 55 处这种写法的缩影。
void PkLogCompatTest::testFreeFunctionPrintfFormWorks()
{
    g_lines.clear();
    const int h = PkLogAddSink(capture, nullptr);
    qDebug("value=%d name=%s", 7, "abc");
    PkLogRemoveSink(h);
    PK_COMPARE(static_cast<int>(g_lines.size()), 1);
    PK_COMPARE(g_lines[0], std::string("value=7 name=abc"));
}

// qPrintable(x)：鸭子类型要求 .PkToUtf8()，真实调用点里 x 是 QString/PkString，
// 这里用一个最小的 fake 类型代替（不引入 pk/string 依赖）。
namespace {
struct FakePkString
{
    std::string s;
    std::string PkToUtf8() const { return s; }
};
} // namespace

void PkLogCompatTest::testQPrintableExtractsUtf8ForPrintfArg()
{
    g_lines.clear();
    const int h = PkLogAddSink(capture, nullptr);
    FakePkString name{"pen"};
    qDebug("tool=%s", qPrintable(name));
    PkLogRemoveSink(h);
    PK_COMPARE(static_cast<int>(g_lines.size()), 1);
    PK_COMPARE(g_lines[0], std::string("tool=pen"));
}

// qInstallMessageHandler：装/卸、返回旧 handler、装上之后确实收到消息。
// 用 PK_VERIFY 而不是 PK_COMPARE 比较函数指针——PK_COMPARE 失败时要把两侧
// 都字符串化，函数指针没有这个诉求，PK_VERIFY 只字符串化 `#statement` 本身。
namespace {
std::vector<std::string> g_handlerMessages;
void testMessageHandler(PkLogLevel, const PkLogContext &, const char *message)
{
    g_handlerMessages.push_back(message);
}
} // namespace

void PkLogCompatTest::testInstallMessageHandlerRoundTrips()
{
    g_handlerMessages.clear();
    QtMessageHandler old = qInstallMessageHandler(testMessageHandler);
    PK_VERIFY(old == nullptr); // 本测试进程此前没装过 handler

    qWarning() << "via-handler";

    QtMessageHandler restored = qInstallMessageHandler(old);
    PK_VERIFY(restored == testMessageHandler);
    PK_COMPARE(static_cast<int>(g_handlerMessages.size()), 1);
    PK_COMPARE(g_handlerMessages[0], std::string("via-handler"));
}

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_compat.inc"

int run_compat_tests(int argc, char **argv)
{
    PkLogCompatTest tc;
    return PkTest::qExec(&tc, argc, argv);
}
