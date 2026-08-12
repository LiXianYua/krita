// 模拟「pk/test/compat/QtGlobal 先进 TU」那条真实的共存路径：在它里面
// qFuzzyCompare / qFuzzyIsNull 是**两个 #define**（→ pkFuzzyCompare /
// pkFuzzyIsNull）。预处理器会把此后任何函数体里写着这两个名字的地方当场改写。
//
// 这里把宏指向一对**语义故意错误**的实现。若 PkSizeF::operator== 的函数体里
// 写的是 qFuzzy* 这两个名字，它就会被改写到破坏版上去，下面那批期望值立刻变红；
// 写的是 pkQtFuzzy*（宏改写不到的名字）才会绿。
//
// 只用真 Qt 也覆盖不到这一条：oracle/ 的编译行里根本没有 pk/test 的垫片，
// 两边都走 Qt 公式，对拍永远发现不了这类**预处理期**的语义偷换。
//
// ── ⚠ 被污染的 include 必须落进匿名 namespace ────────────────────────────
// 完整理由见 point_macro_proof.cpp 的同一段（一句话：这个 TU 编出来的
// operator==(const PkSizeF&, const PkSizeF&) 与 test_size.cpp 那个 TU 编出来的
// 同名同签名、函数体却不同，两者都是 inline → 弱符号 → 链接器只留一份，
// 探针会失去判别力，反过来也可能把干净 TU 的断言变成假红）。
// 匿名 namespace 把整包压成内部链接，链接顺序再影响不了任何东西。
// 纪律只有一条：**系统头必须留在 namespace 之外**（否则会造出 (anonymous)::std）。
//
// 附带的一条：PkSize.cpp 里的 scaled() 是**非 inline** 的（照 Qt 的形态，
// QSize::scaled 定义在 qsize.cpp 里）。落进匿名 namespace 之后它在本 TU 里
// 没有定义 —— 所以本探针不调用 scaled/scale，只测 operator==。真要调，表现是
// 响亮的链接错误，不是静默错行为。

#include "size_macro_proof.h"

#include <limits>

namespace {

// 破坏版：恒返回 false。
bool pkProofBrokenFuzzyCompareS(double, double) { return false; }
bool pkProofBrokenFuzzyCompareS(float, float) { return false; }
bool pkProofBrokenFuzzyIsNullS(double) { return false; }
bool pkProofBrokenFuzzyIsNullS(float) { return false; }

#define qFuzzyCompare pkProofBrokenFuzzyCompareS
#define qFuzzyIsNull  pkProofBrokenFuzzyIsNullS

// PkGlobal.h 检测到 qFuzzyCompare 已是宏时会整段让位，连 qAbs 也不再定义
//（那是给 pk/test 的垫片留的位置）。这里补上与 pk/test 那份逐字相同的 qAbs，
// 复现真实共存路径的形状。
template <typename T> constexpr T qAbs(const T &t) { return t >= T(0) ? t : -t; }

#include "../PkSize.h"

} // 匿名 namespace —— 以上全部是本 TU 私有的内部链接实体

PkSizeMacroProof pkSizeMacroProbe()
{
    const double inf = std::numeric_limits<double>::infinity();
    PkSizeMacroProof p{};
    // 宏真的生效了吗——如果 PkGlobal.h 的让位分支没走到，这一条就是 false，
    // 整个探针会被判成空转（那正是「测了个寂寞」的形态）。
    p.sabotagedFuzzyWasVisible = (qFuzzyCompare(1.0, 1.0) == false)
                              && (qFuzzyIsNull(0.0) == false);
    p.nearIsEqual        =  (PkSizeF(1.0, 1.0) == PkSizeF(1.0 + 1e-13, 1.0));
    p.farIsNotEqual      = !(PkSizeF(1.0, 1.0) == PkSizeF(1.0 + 1e-11, 1.0));
    p.zeroSideIsNotEqual = !(PkSizeF(0.0, 0.0) == PkSizeF(1e-300, 0.0));
    p.bothZeroIsEqual    =  (PkSizeF(0.0, 0.0) == PkSizeF(0.0, 0.0));
    p.infVsInfIsNotEqual = !(PkSizeF(inf, 1.0) == PkSizeF(inf, 1.0));
    p.infVsNegInfIsEqual =  (PkSizeF(inf, 1.0) == PkSizeF(-inf, 1.0));
    return p;
}
