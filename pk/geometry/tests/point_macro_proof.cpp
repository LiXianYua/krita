// 模拟「pk/test/compat/QtGlobal 先进 TU」那条真实的共存路径：在它里面
// qFuzzyCompare / qFuzzyIsNull 是**两个 #define**（→ pkFuzzyCompare /
// pkFuzzyIsNull）。预处理器会把此后任何函数体里写着这两个名字的地方当场改写。
//
// 这里把宏指向一对**语义故意错误**的实现。若 PkPointF::operator== 的函数体里
// 写的是 qFuzzy* 这两个名字，它就会被改写到破坏版上去，下面那批期望值立刻变红；
// 写的是 pkQtFuzzy*（宏改写不到的名字）才会绿。
//
// 只用真 Qt 也覆盖不到这一条：oracle/ 的编译行里根本没有 pk/test 的垫片，
// 两边都走 Qt 公式，对拍永远发现不了这类**预处理期**的语义偷换。

#include "point_macro_proof.h"

#include <limits>

// 破坏版：恒返回 false。真实的 pkFuzzyCompare/pkFuzzyIsNull 现在与 Qt 语义
// 一致（Task 1 已核对），拿它们当诱饵测不出东西，所以用一对必定不同的实现。
static bool pkProofBrokenFuzzyCompare(double, double) { return false; }
static bool pkProofBrokenFuzzyCompare(float, float) { return false; }
static bool pkProofBrokenFuzzyIsNull(double) { return false; }
static bool pkProofBrokenFuzzyIsNull(float) { return false; }

#define qFuzzyCompare pkProofBrokenFuzzyCompare
#define qFuzzyIsNull  pkProofBrokenFuzzyIsNull

// PkGlobal.h 检测到 qFuzzyCompare 已是宏时会整段让位，连 qAbs 也不再定义
//（那是给 pk/test 的垫片留的位置）。这里补上与 pk/test 那份逐字相同的 qAbs，
// 复现真实共存路径的形状。
template <typename T> constexpr T qAbs(const T &t) { return t >= T(0) ? t : -t; }

#include "../PkPoint.h"

PkPointMacroProof pkPointMacroProbe()
{
    const double inf = std::numeric_limits<double>::infinity();
    PkPointMacroProof p{};
    // 宏真的生效了吗——如果 PkGlobal.h 的让位分支没走到，这一条就是 false，
    // 整个探针会被判成空转（那正是「测了个寂寞」的形态）。
    p.sabotagedFuzzyWasVisible = (qFuzzyCompare(1.0, 1.0) == false)
                              && (qFuzzyIsNull(0.0) == false);
    p.nearIsEqual        =  (PkPointF(1.0, 1.0) == PkPointF(1.0 + 1e-13, 1.0));
    p.farIsNotEqual      = !(PkPointF(1.0, 1.0) == PkPointF(1.0 + 1e-11, 1.0));
    p.zeroSideIsEqual    =  (PkPointF(0.0, 0.0) == PkPointF(1e-300, 0.0));
    p.infVsInfIsNotEqual = !(PkPointF(inf, 0.0) == PkPointF(inf, 0.0));
    p.infVsNegInfIsEqual =  (PkPointF(inf, 0.0) == PkPointF(-inf, 0.0));
    return p;
}
