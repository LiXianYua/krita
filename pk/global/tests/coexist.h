#pragma once

// 「三份标量源（pk/test、pk/geometry、pk/global）共存」的探针。
//
// 三份 compat/QtGlobal 同名，试接时编译行会同时有 -I pk/test/compat、
// -I pk/geometry/compat、-I pk/global/compat。谁先进翻译单元、三份会不会互相
// 重定义，是 R-18 真会撞上的问题——这里用三个独立 TU 把三种「入口垫片」各编译
// 一遍：
//
//   tests/coexist_test_first.cpp     pk/test 那份 compat 先进 TU
//   tests/coexist_geometry_first.cpp pk/geometry 那份 compat 先进 TU
//   tests/coexist_global_first.cpp   pk/global 那份 compat 先进 TU
//
// 每个 TU 都经 compat 垫片链把三份全部拉进来（global 的 compat 自己先拉 pktest
// 再拉 geometry；geometry 的 compat 自己先拉 pktest），PkGlobal.h 总在最后落地、
// 总能让位。**能编过本身就是断言的一半**（重复定义 qAbs/qRound/pkQtFuzzy* 是硬
// 错误，qFuzzyCompare 那个 #define 打架会把函数名当场改写掉）；另一半是三条路径
// 必须给出同一组取值，且与真 Qt 5.15.7 一致——由 tests/test_global.cpp 的三个
// 测试函数核对。
//
// ⚠ 探针取值刻意选在三条路径间不敏感的输入上：
//   · qFuzzyCompare 在这三条路径上全部让位给了 pk/test 的 #define
//     → pkFuzzyCompare（geometry 的 qFuzzy* 函数被 PK_GLOBAL_SCALARS_FROM_PKTEST
//     跳过）。取 (1.0, 1.0) 时 pkFuzzyCompare 与 Qt 公式都返回 true，路径无关。
//   · qAbs 全部来自 pk/test（`t >= T(0)`），与 Qt 的 `t >= 0` 对 -3 等价。
//   · qRound / qMin / qreal 来自 pk/geometry。
// 取值与真 Qt 的对齐由 test_global.cpp 的独立断言（不经让位路径）钉住；这里只
// 证明「让位之后三条路径给同一组值」。
//
// 本头**不得** include 任何提供 qAbs/qFuzzy*/qRound/qreal 的东西：三个 TU 的
// include 顺序正是被测变量，这里多包一个头就把变量污染了。<type_traits> 不提供
// 其中任何一项，安全。

#include <type_traits>

struct PkGlobalCoexistProbe
{
    int absNeg;        // qAbs(-3)                    → 3
    int roundHalfNeg;  // qRound(-1.5)                → -1（负半值向 +∞）
    bool fuzzyEqual;   // qFuzzyCompare(1.0, 1.0)     → true
    int minPair;       // qMin(2, 3)                  → 2
    int qrealInt;      // (int)(qreal)1.5             → 1
};

PkGlobalCoexistProbe pkCoexistGlobalShimFirst();
PkGlobalCoexistProbe pkCoexistGeometryShimFirst();
PkGlobalCoexistProbe pkCoexistTestShimFirst();

// ── 匿名 namespace 的必要性（与 R-03 的 coexist.h 同判据）────────────────────
//
// 三个 TU 里 qAbs 都来自 pk/test 那份垫片（`t >= T(0)`），而 test_global.cpp 里
// 的 qAbs 来自 PkGlobal.h（`t >= 0`）——**同名同签名、函数体不同**，且都是
// inline/模板，以弱符号发射。链接器只保留其中一份，于是：
//   · 多个 TU 实际调用的是同一份实现，「每种 include 顺序各测一遍」不成立；
//   · 谁赢由链接顺序决定，换链接器 / 加 -flto / 改源文件顺序都可能翻盘。
// 把三份 compat 包进匿名 namespace，它们落地的一切（qAbs / qRound / qMin /
// pkQtFuzzy* / qFuzzy*…）都变成本 TU 私有的内部链接实体，探针测的就真是自己
// 这条 include 路径编出来的那份。纪律只有一条：**系统头留在 namespace 之外**
//（PkTestCompare.h 与 <limits> 由各 TU 在 namespace 外先 include）。
//
// 三个 TU 的函数体一模一样，只有它们上方的 include 顺序不同——用宏共享函数体，
// 免得三份手抄的取值悄悄跑偏，把「顺序无关」这一条测试变成三个不同的测试。
#define PK_GLOBAL_COEXIST_DEFINE(fnName)                    \
    PkGlobalCoexistProbe fnName()                           \
    {                                                       \
        PkGlobalCoexistProbe p;                             \
        p.absNeg = qAbs(-3);                                \
        p.roundHalfNeg = qRound(-1.5);                      \
        p.fuzzyEqual = qFuzzyCompare(1.0, 1.0);             \
        p.minPair = qMin(2, 3);                             \
        p.qrealInt = static_cast<int>(static_cast<qreal>(1.5)); \
        return p;                                           \
    }
