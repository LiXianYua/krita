#pragma once

// 「两份 compat/QtGlobal 共存」的探针。
//
// pk/test/compat/QtGlobal（R-11 交付、本任务不许改）与 pk/geometry/compat/QtGlobal
// 同名，试接时编译行会同时有 -I pk/test/compat 与 -I pk/geometry/compat。
// 谁先进翻译单元、两份会不会互相重定义，是 Task 7 真会撞上的问题——这里用三个
// 独立 TU 把三种顺序各编译一遍：
//
//   tests/coexist_test_first.cpp      pk/test 那份在前
//   tests/coexist_geometry_first.cpp  pk/geometry 那份在前
//   tests/coexist_pkglobal_first.cpp  先直接包库头 PkGlobal.h，之后才撞上 pk/test 那份
//
// **能编过本身就是断言的一半**（重复定义 qAbs 是硬错误，qFuzzyCompare 那个
// #define 打架会把函数名当场改写掉）；另一半是三条路径必须给出同一组取值，
// 且与真 Qt 5.15.7 一致——由 tests/test_global.cpp 的三个测试函数核对。
//
// 本头**不得** include 任何提供 qAbs/qFuzzy*/qRound/qreal 的东西：三个 TU 的
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
    bool fuzzyNull;        // qFuzzyIsNull(0.0)
    bool fuzzyNotNull;     // qFuzzyIsNull(1e-11)
    unsigned long qrealSize;
    bool qrealIsDouble;
};

PkCoexistProbe pkCoexistTestShimFirst();
PkCoexistProbe pkCoexistGeometryShimFirst();
PkCoexistProbe pkCoexistPkGlobalFirst();

// 三个 TU 的函数体一模一样，只有它们上方的 include 顺序不同——用宏共享函数体，
// 免得三份手抄的取值悄悄跑偏，把「顺序无关」这一条测试变成三个不同的测试。
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
        p.fuzzyNull = qFuzzyIsNull(0.0);                                \
        p.fuzzyNotNull = qFuzzyIsNull(1e-11);                           \
        p.qrealSize = sizeof(qreal);                                    \
        p.qrealIsDouble = std::is_same<qreal, double>::value;           \
        return p;                                                       \
    }
