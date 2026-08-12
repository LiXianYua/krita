#pragma once
// ============================================================================
// 试接垫片 —— **不是 R-03 的交付物**。`kis_debug.h` 的真正归属是 **R-08（日志与
// 调试设施）**。
//
// 真品是 libs/global/kis_debug.h，184 行，`#include <QDebug>` + `<QLoggingCategory>`，
// 定义了 41 个 qCDebug 日志分类（dbgKrita/dbgImage/dbgUI/…）与一整套 ppVar/
// __METHOD_NAME__ 机制。整个 Qt 日志分类系统不可能用最小垫片复刻，也不该由 R-03 复刻。
//
// 两个试接目标真正用到的只有四样（口径：两个测试 .cpp + 被测源全文 grep）：
//   · dbgKrita        —— KisFourPointInterpolatorTest.cpp 里 7 次
//   · ppVar           —— 同上 32 次（都跟在 dbgKrita 或 ENTER_FUNCTION 后面）
//   · ENTER_FUNCTION  —— 同上 4 次，**四次全都带 `<< ...` 续接**
//     （如 :220 `ENTER_FUNCTION() << "R:" << ppVar(pt) << ...`）。真品展开成
//     `qDebug() << "Entering" << __METHOD_NAME__`，是个能继续 << 的流对象，
//     所以垫片也必须返回流对象 —— 第一版写成 `((void)0)` 当场报
//     "invalid operands of types 'void' and 'const char [3]'"，试接压出来的。
//   · qCritical       —— KisFourPointInterpolatorTest.cpp:328 一次，**不在**
//     FPIB_DEBUG 里，是测试自己的诊断输出。它来自真品 kis_debug.h:9 传递
//     include 的 <QDebug>，所以垫片也要复刻这条传递性。
// libs/global/KisRectsGridTest.cpp:9 与 KisRectsGrid.cpp:15 也 include 它，
// 但那两处一次都没调用（KisRectsGridTest.cpp:44 那行 qDebug 是注释掉的）。
//
// **dbgKrita 是吞掉一切的 sink，不是转发到 stderr。** 理由：两个目标都不断言
// 调试输出，而 harness 用 stdout 的 PASS/FAIL 行做判定，真把 32 处 ppVar 打出来
// 只会淹掉判定行。想看调试输出的人应该去跑真 Krita，不是跑试接。
//
// sink 用模板 operator<< 而不是给每个类型写重载：这样 `dbgKrita << ppVar(pt)`
// 里的 PkPointF 不需要有 operator<<（它确实没有），试接也不会因为"调试输出
// 打不出来"这种与 API 形状无关的理由变红。
// ============================================================================
#include <QtGlobal>

// 真品 kis_debug.h:182 在文件末尾 include kis_assert.h，于是 include 了
// kis_debug.h 的调用点顺带拿到 KIS_ASSERT 家族。这条传递性照复刻 —— 漏掉的话
// "某个 .cpp 只 include 了 kis_debug.h 却能用 KIS_ASSERT" 会被误判成调用点的错。
// 这里解析到的是**真** libs/global/kis_assert.h（本目录没有同名垫片）。
#include <kis_assert.h>

struct PkGraftDebugSink {
    template <typename T>
    PkGraftDebugSink &operator<<(const T &) { return *this; }
};

// kis_debug.h:45 `#define dbgKrita qCDebug(_41000)` —— 同样是"每次求值现造一个
// 流对象"的形态，所以这里也写成构造表达式而不是全局变量。
#define dbgKrita PkGraftDebugSink()

// 真品 kis_debug.h:9 `#include <QDebug>` 传递给下游的 Qt 日志函数。只补试接
// 目标真的写出来的那几个 —— qCritical 有 1 处真实调用点（见头注释），
// qDebug 是真品 ENTER_FUNCTION 宏体里用的，qWarning 一并带上是因为它与前两个
// 在 <QDebug> 里是同一批、少一个下一个目标就要回来补。
inline PkGraftDebugSink qDebug() { return PkGraftDebugSink(); }
inline PkGraftDebugSink qWarning() { return PkGraftDebugSink(); }
inline PkGraftDebugSink qCritical() { return PkGraftDebugSink(); }

// kis_debug.h:155 逐字照抄。
#define ppVar( var ) #var << "=" << (var)

// kis_debug.h:175/178 是 `qDebug() << "Entering" << __METHOD_NAME__`。
// 形态照抄（必须返回流对象，见头注释），只把 __METHOD_NAME__ 换成标准的
// __func__ —— 真品那个宏是 __PRETTY_FUNCTION__ 的字符串切割，而输出反正被
// sink 吞掉，切得准不准不进入任何断言。
#define ENTER_FUNCTION() qDebug() << "Entering" << __func__
