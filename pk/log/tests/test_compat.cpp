#include "test_compat.h"

// 顶部真的写 Qt 拼法——走 -I pk/log/compat 解析到我们的实现，不是拿 Pk 名字
// 抄一遍同样的测试（brief 的硬要求：这才证明垫片"一个字都不改"真的能编过）。
#include <QDebug>
#include <QLoggingCategory>

#include "../PkLogSink.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <unistd.h>

// 评审 Important 3：钉住"用 #define 而非 using"这条设计不变量。真实 Krita
// 头里有 `class QDebug;` 前置声明的例子（libs/global/KoZoomMode.h:14、
// libs/flake/text/KoSvgText.h:28 等）。`#define QDebug PkDebug` 把这行原样
// 改写成 `class PkDebug;`——对已经完整定义过的类重复前置声明合法。如果垫片
// 改成 `using QDebug = PkDebug;`，这里会报 "using typedef-name 'QDebug'
// after 'class'" 编译失败——这才是这条测试要守住的东西，不是运行期断言。
class QDebug;
QDebug *g_pkLogTestForwardDeclaredQDebugPtr = nullptr;

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

// 评审 Critical 项：既有的全部断言都挂在 PkLogAddSink 通道上——PkLogEmit 先
// 分发 sink 再喂 spdlog，"default" 分类没建 logger 时消息仍然到 sink（旧 bug
// 因此躲过了七轮评审），但 spdlog 侧真的丢了。这里改用 test_sink.cpp 已有的
// dup/dup2/mkstemp 手法接管 stderr，直接断言 spdlog 的彩色 stderr sink 真的
// 打出了这条消息文本，而不是只打"查不到 logger"的诊断行。
void PkLogCompatTest::testFreeFunctionLogReachesSpdlogNotJustSink()
{
    fflush(stderr);
    char path[] = "/tmp/pklog_test_stderr_XXXXXX";
    const int tmpFd = mkstemp(path);
    PK_VERIFY(tmpFd >= 0);
    const int savedStderr = dup(STDERR_FILENO);
    PK_VERIFY(savedStderr >= 0);
    dup2(tmpFd, STDERR_FILENO);
    close(tmpFd);

    // qWarning() 无分类：ctx.category 落成 "default"（PkMessageLogger.cpp:39）。
    qWarning() << "reaches-spdlog-default-category";

    fflush(stderr);
    dup2(savedStderr, STDERR_FILENO);
    close(savedStderr);

    std::string capturedStderr;
    FILE *f = fopen(path, "rb");
    PK_VERIFY(f != nullptr);
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        capturedStderr.append(buf, n);
    }
    fclose(f);
    std::remove(path);

    PK_VERIFY(capturedStderr.find("reaches-spdlog-default-category") != std::string::npos);
    // 走的是 spdlog 的正常落盘路径，不是"查不到 logger"的诊断分支。
    PK_VERIFY(capturedStderr.find("PkLogEmit: no logger") == std::string::npos);
}

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_compat.inc"

int run_compat_tests(int argc, char **argv)
{
    PkLogCompatTest tc;
    return PkTest::qExec(&tc, argc, argv);
}
