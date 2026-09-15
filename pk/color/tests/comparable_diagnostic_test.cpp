// pk/color 的 R-87 判据：`PK_COMPARE(PkColor …)` 判红时，失败信息里必须**打印出
// 实际值**（`PkColor(ARGB 1, 1, 0, 0)`），不再是 `<unprintable>`。
//
// 为什么自带 main() 而不并进 test_pkcolor：本判据要**故意制造一次失败**，再把
// `PkTestCase::recordFailure` 打到 stdout 的整段文本截回来核对；并进去会让那个
// 二进制里多出一次真实失败（它的 Totals 会变红）。形态照抄
// pk/geometry/tests/comparable_diagnostic_test.cpp（同一任务、同一形态）。
//
// 判据的值文本口径（与 PkColor.h 的那条 ostream 运算符、README 偏离 9 同源）：
// 真 Qt `QDebug operator<<(QDebug, const QColor&)` 的原文，只把类型名换成 Pk。
// ⚠ 真 Qt 的 `QCOMPARE(QColor, QColor)` **打的是 `<unprintable>`** —— 本判据钉住的
//   是一条**超出 Qt** 的行为（Q2-a 裁定），不是对齐 Qt。
#include "PkColor.h"
#include "PkTest.h"

#include <cstdio>
#include <string>
#include <unistd.h>   // dup / dup2 / close / fileno

// ---- 故意失败的用例（判据本体）----
struct PkColorCompareCase : public PkTestObject
{
    void run() { PK_COMPARE(PkColor(255, 0, 0), PkColor(0, 255, 0)); }
};

static int g_failures = 0;

static void expect(bool ok, int lineNo, const char *what)
{
    if (ok) {
        std::printf("ok   %-3d %s\n", lineNo, what);
        return;
    }
    ++g_failures;
    std::printf("FAIL %-3d %s\n", lineNo, what);
}

// 跑 case.run()，把这一段 stdout（fd 1）截回来。
// `PkTestCase::recordFailure` 的落盘点是 `std::printf`（pk/test/PkTestCase.cpp），
// 所以 dup2 到 tmpfile 即可捕获；缓冲是 FILE* 级的，dup2 前后各 fflush 一次。
template <typename Case>
static std::string captureRun(Case &c, const char *className)
{
    std::fflush(stdout);
    FILE *tmp = std::tmpfile();
    if (!tmp) {
        ++g_failures;
        std::printf("FAIL      tmpfile() 建不出来\n");
        return std::string();
    }
    const int savedFd = dup(fileno(stdout));
    dup2(fileno(tmp), fileno(stdout));

    PkTestCase::current().beginFunction(className, "run");
    c.run();
    PkTestCase::current().endFunction();

    std::fflush(stdout);
    dup2(savedFd, fileno(stdout));
    close(savedFd);

    std::string out;
    std::rewind(tmp);
    char buf[4096];
    const std::size_t n = std::fread(buf, 1, sizeof(buf) - 1, tmp);
    out.append(buf, n);
    std::fclose(tmp);
    return out;
}

int main()
{
    // 进程级单例的计数归零，免得残留状态影响判据
    PkTestCase::current().beginRun();

    PkColorCompareCase c;
    const std::string captured = captureRun(c, "PkColorCompareCase");

    // 原文即证据：人可直接看到失败信息里到底有没有值
    std::printf("---- 截获的失败信息原文 ----\n%s----------------------------\n",
                captured.c_str());

    // 判据 1/2：两个实际值都在（形态 = 真 Qt QDebug 通道的原文，类型名换成 Pk）
    expect(captured.find("PkColor(ARGB 1, 1, 0, 0)") != std::string::npos,
           __LINE__, "失败信息含 Actual 值 PkColor(ARGB 1, 1, 0, 0)");
    expect(captured.find("PkColor(ARGB 1, 0, 1, 0)") != std::string::npos,
           __LINE__, "失败信息含 Expected 值 PkColor(ARGB 1, 0, 1, 0)");

    // 判据 3：不再退化成占位文本
    expect(captured.find("<unprintable>") == std::string::npos,
           __LINE__, "失败信息里不含 <unprintable>");

    // 判据 4：文本的另一支（Invalid）也是照 Qt 原文给的，不是空串
    expect(pkTestToString(PkColor()) == "PkColor(Invalid)",
           __LINE__, "无效色按真 Qt 的 QDebug 原文给 PkColor(Invalid)");

    // 判据 5（对照组，防「无脑点亮」）：没有 ostream 运算符的类型仍退化成占位文本
    struct Opaque { int x; };
    expect(pkTestToString(Opaque{1}) == "<unprintable>",
           __LINE__, "无可插入运算符的类型仍退化为 <unprintable>（判据没被无脑点亮）");

    std::printf("\n%s (%d failure(s))\n", g_failures ? "FAILED" : "PASSED", g_failures);
    return g_failures ? 1 : 0;
}
