// pk/geometry 调试流运算符的期望值断言。
//
// 为什么自带 main() 而不是并进 test_pkgeometry：本探测需要自己提供 PkLogEmit
// 的实现来截获整行输出，而 test_pkgeometry 的链接行不该因此背上 pk/log 的实现
// （PkDebug.cpp 的 flush 会调到 PkLogEmit）。
//
// 期望值的来源：同目录 ../../oracle/debugstream_qt.cpp 在真 Qt 5.15.7 上跑出来的
// 原文，逐字照抄，**只把类型名从 Q* 换成 pk 自己的 Pk\***（理由见 README 的登记偏离）。
// 唯一例外：PkPolygon 的用例，真 Qt 探针里用 QPolygon(1,2 3,4 5,6) 建的是三个点，
// 本文件的构造与它逐点对齐。
#include "PkPoint.h"
#include "PkSize.h"
#include "PkRect.h"
#include "PkLine.h"
#include "PkMargins.h"
#include "PkPolygon.h"

#include "PkDebug.h"
#include "PkLogBackend.h"   // PkLogEmit 的**声明**——定义必须与它逐字同签名，
                            // 否则链接期对不上 mangled 名，或撞 ODR

#include <cstdio>
#include <string>
#include <vector>

// ---- 截获 PkDebug 的整行输出（唯一需要的后端符号）----
// PkDebug.cpp 的 flush 只调 PkLogEmit 这一个后端符号，所以本 TU 定义它就够了：
// 不需要 PkLogSink.cpp，也不需要 PkLogBackend.cpp（后者会拖进 spdlog）。
static std::vector<std::string> g_lines;
void PkLogEmit(PkLogLevel, const PkLogContext &, const std::string &message)
{
    g_lines.push_back(message);
}

static int g_failures = 0;

static void expect(int lineNo, const std::string &expr, const std::string &got,
                   const std::string &want)
{
    if (got == want) {
        std::printf("ok   %-2d %s\n", lineNo, expr.c_str());
        return;
    }
    ++g_failures;
    std::printf("FAIL %-2d %s\n     got  <%s>\n     want <%s>\n",
                lineNo, expr.c_str(), got.c_str(), want.c_str());
}

// 每次用一个新的 PkDebug（PkDebugMakeForTest），析构即 flush，正好收一行。
#define CHECK(expr, want)                                                   \
    do {                                                                    \
        g_lines.clear();                                                     \
        { PkDebug dbg = PkDebugMakeForTest(PkLogDebug, "probe"); dbg << expr; } \
        expect(__LINE__, #expr, g_lines.empty() ? std::string("<no line>") : g_lines.front(), want); \
    } while (false)

int main()
{
    CHECK(PkPoint(1, 2),        std::string("PkPoint(1,2)"));
    CHECK(PkPointF(1, 2),       std::string("PkPointF(1,2)"));
    CHECK(PkPointF(-1.5, 2.25), std::string("PkPointF(-1.5,2.25)"));
    CHECK(PkSize(1, 2),         std::string("PkSize(1, 2)"));
    CHECK(PkSizeF(1, 2),        std::string("PkSizeF(1, 2)"));
    CHECK(PkRect(1, 2, 3, 4),   std::string("PkRect(1,2 3x4)"));
    CHECK(PkRectF(1, 2, 3, 4),  std::string("PkRectF(1,2 3x4)"));
    CHECK(PkLine(1, 2, 3, 4),   std::string("PkLine(PkPoint(1,2),PkPoint(3,4))"));
    CHECK(PkLineF(1, 2, 3, 4),  std::string("PkLineF(PkPointF(1,2),PkPointF(3,4))"));
    CHECK(PkMargins(1, 2, 3, 4), std::string("PkMargins(1, 2, 3, 4)"));
    CHECK(PkMarginsF(1, 2, 3, 4), std::string("PkMarginsF(1, 2, 3, 4)"));

    PkPolygon poly;
    poly << PkPoint(1, 2) << PkPoint(3, 4) << PkPoint(5, 6);
    CHECK(poly, std::string("PkPolygon(PkPoint(1,2)PkPoint(3,4)PkPoint(5,6))"));
    CHECK(PkPolygon(), std::string("PkPolygon()"));

    PkPolygonF polyf;
    polyf << PkPointF(1, 2) << PkPointF(3, 4) << PkPointF(5, 6);
    CHECK(polyf, std::string("PkPolygonF(PkPointF(1,2)PkPointF(3,4)PkPointF(5,6))"));
    CHECK(PkPolygonF(), std::string("PkPolygonF()"));

    // 链式：分隔符由各分量自己的 maybeSpace 负责，运算符不得额外吐空格。
    CHECK(PkPointF(1, 2) << PkLineF(1, 2, 3, 4) << PkRectF(1, 2, 3, 4),
          std::string("PkPointF(1,2) PkLineF(PkPointF(1,2),PkPointF(3,4)) PkRectF(1,2 3x4)"));

    // 与普通字符串混排：前后各恰好一个空格。
    {
        g_lines.clear();
        {
            PkDebug dbg = PkDebugMakeForTest(PkLogDebug, "probe");
            dbg << "a" << PkPointF(1, 2) << "b";
        }
        expect(__LINE__, "\"a\" << PkPointF(1,2) << \"b\"",
               g_lines.empty() ? std::string("<no line>") : g_lines.front(),
               std::string("a PkPointF(1,2) b"));
    }

    // nospace 上下文：运算符不得打破调用方的 nospace（真 Qt 用 QDebugStateSaver 保证）。
    {
        g_lines.clear();
        {
            PkDebug dbg = PkDebugMakeForTest(PkLogDebug, "probe");
            dbg.nospace() << "Z" << PkPoint(0, 0) << PkPointF(0, 0);
        }
        expect(__LINE__, "nospace: \"Z\" << PkPoint(0,0) << PkPointF(0,0)",
               g_lines.empty() ? std::string("<no line>") : g_lines.front(),
               std::string("ZPkPoint(0,0)PkPointF(0,0)"));
    }

    std::printf("\n%s (%d failure(s))\n", g_failures ? "FAILED" : "PASSED", g_failures);
    return g_failures ? 1 : 0;
}
