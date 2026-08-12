#pragma once

// 「两份 compat/QtGlobal 共存」的探针。
//
// pk/test/compat/QtGlobal（R-11 交付、本任务不许改）与 pk/geometry/compat/QtGlobal
// 同名，试接时编译行会同时有 -I pk/test/compat 与 -I pk/geometry/compat。
// 谁先进翻译单元、两份会不会互相重定义，是 Task 7 真会撞上的问题——这里用两个
// 独立 TU 把两种顺序各编译一遍：
//
//   tests/coexist_test_first.cpp          pk/test 那份在前（走 PkGlobal.h 的机制①）
//   tests/coexist_geometry_first.cpp      pk/geometry 那份在前（走机制②）
//   tests/coexist_compat_rect_first.cpp   compat/QRect 一类**类型垫片**在前
//                                         —— 真实调用点的顺序，见该文件顶部
//
// **能编过本身就是断言的一半**（重复定义 qAbs 是硬错误，qFuzzyCompare 那个
// #define 打架会把函数名当场改写掉）；另一半是两条路径必须给出同一组取值，
// 且与真 Qt 5.15.7 一致——由 tests/test_global.cpp 的两个测试函数核对。
//
// **另一半为什么必须包含零侧语义**：这两条路径上 qFuzzyCompare 都让位给了
// pk/test 的 pkFuzzyCompare，而 pk/test 不在 R-03 的 locks 里、R-11 随时可能动它。
// 探针只取非零点（1.0 vs 1.0、1.0 vs 1.0000001）时，给 pkFuzzyCompare 注入一个
// 「任一侧为 0 就走 fuzzyIsNull」的分支——一条真实的对 Qt 偏离——这些 TU 会全绿。
// 所以 fuzzyZeroA/fuzzyZeroB 是必需项：Qt 的右端取 qMin(|p1|,|p2|)，任一侧为 0
// 就恒 false，让位路径必须给出同样的 false。
//
// 本头**不得** include 任何提供 qAbs/qFuzzy*/qRound/qreal 的东西：两个 TU 的
// include 顺序正是被测变量，这里多包一个头就把变量污染了。<type_traits> 不提供
// 其中任何一项，安全。

#include <type_traits>

struct PkCoexistProbe
{
    double absNeg;         // qAbs(-2.5)
    int roundHalfPos;      // qRound(0.5)
    int roundHalfNeg;      // qRound(-0.5)
    int boundAbove;        // qBound(0, 5, 3)
    bool fuzzyEqual;       // qFuzzyCompare(1.0, 1.0)
    bool fuzzyDiffer;      // qFuzzyCompare(1.0, 1.0000001)
    bool fuzzyZeroA;       // qFuzzyCompare(0.0, 1e-300)  —— Qt 恒 false
    bool fuzzyZeroB;       // qFuzzyCompare(1e-300, 0.0)  —— Qt 恒 false
    bool fuzzyNull;        // qFuzzyIsNull(0.0)
    bool fuzzyNotNull;     // qFuzzyIsNull(1e-11)
    unsigned long qrealSize;
    bool qrealIsDouble;
};

PkCoexistProbe pkCoexistTestShimFirst();
PkCoexistProbe pkCoexistGeometryShimFirst();

// ── 第三条路径的探针 ────────────────────────────────────────────────────────
//
// 与上面两个不同，它**不测标量取值**，测的是 compat/ 的传递 include 纪律
//（「每个类型垫片先包 compat/QtGlobal」「qrect.h 包 qsize.h/qpoint.h」）。
// 两条被守的事都在 coexist_compat_rect_first.cpp 编译期就见分晓；下面这几个
// 取值是为了让那个 TU 必须真的被链接进可执行文件、且被一个测试函数调用 ——
// 否则它可以被悄悄从 CMakeLists 里漏掉而没人发现。
//
// 取值刻意只用整数/浮点四则，**不碰 qAbs / qFuzzy* / qRound**：那个 TU 里
// 这些名字来自 pk/test 那份垫片、而别的 TU 里来自 PkGlobal.h，函数体不同、
// 都以弱符号发射 —— 一旦 odr-use 就是 coexist.h 末尾说的那类 ODR 违反。
// 该 TU 因此也不能整体塞进匿名 namespace（PkRect/PkRectF 的类定义会跟着变成
// 内部链接，与 PkRect.cpp 里的 out-of-line 成员对不上）。
struct PkCompatIncludeProbe
{
    int rectRight;      // QRect(QPoint(1,2), QSize(3,4)).right()      → 3
    int rectBottom;     // 同上 .bottom()                              → 5
    double rectFRight;  // QRectF(QPointF(1.5,2.5), QSizeF(3.5,4.5)).right()  → 5.0
    double rectFBottom; // 同上 .bottom()                              → 7.0
};

PkCompatIncludeProbe pkCompatRectFirstProbe();

// ── 为什么两个探针 TU 都要匿名 namespace（**Task 3–6 抄这个形状**）──────────
//
// 两个 TU 里 `qAbs` 都来自 pk/test 那份垫片（`t >= T(0)`），而 test_point.cpp /
// test_global.cpp 里的 `qAbs` 来自 PkGlobal.h（`t >= 0`）——**同名同签名、函数体
// 不同**，且都是 inline/模板，以弱符号发射。链接器只保留其中一份，于是：
//   · 三个 TU 实际调用的是同一份实现，「两种 include 顺序各测一遍」这句话不成立；
//   · 谁赢由链接顺序决定，换链接器 / 加 -flto / 改源文件顺序都可能翻盘（这类
//     ODR 违反在 point_macro_proof.cpp 上已实测复现，见那个文件顶部）。
// 把两份 compat/QtGlobal 包进匿名 namespace，它们落地的一切（qAbs / qRound /
// qMin / pkQtFuzzy* …）都变成本 TU 私有的内部链接实体，探针测的就真是自己这条
// include 路径编出来的那份。纪律只有一条：**系统头留在 namespace 之外**。
//
// 两个 TU 的函数体一模一样，只有它们上方的 include 顺序不同——用宏共享函数体，
// 免得两份手抄的取值悄悄跑偏，把「顺序无关」这一条测试变成两个不同的测试。
#define PK_COEXIST_DEFINE(fnName)                                       \
    PkCoexistProbe fnName()                                             \
    {                                                                   \
        /* qBound 照抄 Qt 的签名返回 const T&，实参是字面量时返回的引用只在   */ \
        /* 本条 full-expression 内有效——先拷进具名 int 再存，别把悬垂当结果。 */ \
        const int boundAbove_ = qBound(0, 5, 3);                        \
        PkCoexistProbe p;                                               \
        p.absNeg = qAbs(-2.5);                                          \
        p.roundHalfPos = qRound(0.5);                                   \
        p.roundHalfNeg = qRound(-0.5);                                  \
        p.boundAbove = boundAbove_;                                     \
        p.fuzzyEqual = qFuzzyCompare(1.0, 1.0);                         \
        p.fuzzyDiffer = qFuzzyCompare(1.0, 1.0000001);                  \
        /* 零侧：Qt 的右端是 qMin(|p1|,|p2|)，任一侧为 0 就恒 false。    */ \
        /* 两个方向都取，因为「只有一侧特判」的实现只会漏其中一个。      */ \
        p.fuzzyZeroA = qFuzzyCompare(0.0, 1e-300);                      \
        p.fuzzyZeroB = qFuzzyCompare(1e-300, 0.0);                      \
        p.fuzzyNull = qFuzzyIsNull(0.0);                                \
        p.fuzzyNotNull = qFuzzyIsNull(1e-11);                           \
        p.qrealSize = sizeof(qreal);                                    \
        p.qrealIsDouble = std::is_same<qreal, double>::value;           \
        return p;                                                       \
    }
