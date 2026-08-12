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
//
// ── ⚠ 被污染的 include 必须落进匿名 namespace（**Task 3–6 照抄这个形状**）──
//
// 这个 TU 在预处理期改写了 PkPoint.h 的含义，于是它编出来的
// `operator==(const PkPointF&, const PkPointF&)` 与 test_point.cpp 那个 TU 编出来
// 的**同名同签名、函数体却不同**。两者都是 inline，都以**弱符号**发射，链接器
// 只保留其中一份 —— 这有两个后果，都实测复现过：
//
//   ① 探针失去判别力：把 PkPoint.h 的 pkQtFuzzy* 改回 qFuzzy*（正是本探针存在的
//      唯一理由）之后，链接器恰好留的是 test_point.cpp 那份**干净**定义，探针里
//      的 `PkPointF(…) == PkPointF(…)` 调到的根本不是本 TU 编出来的那份，
//      28 条单测与对拍**一起绿灯放过**。
//   ② 反过来也成立：源码一字不改，只把本 TU 的 .o 挪到链接行最前重链，恒 false
//      的 operator== 就会赢，把 test_point.cpp 里三条干净断言变红。今天是绿的
//      纯属链接顺序的偶然；换链接器、加 -flto、改源文件顺序都可能翻盘。
//
// 匿名 namespace 把这一整包（PkPointF、它的运算符、qAbs、pkQtFuzzy* …）压成
// **内部链接**：符号是本 TU 私有的局部符号，不参与跨 TU 选择，链接顺序再也影响
// 不了任何东西，而探针调用的一定是自己编出来的那份。
// 代价只有一条纪律：**系统头必须留在 namespace 之外**（否则会造出
// (anonymous)::std）。PkPoint.h / PkGlobal.h 一个系统头都不包，所以这里只需要把
// 本文件自己要用的 <limits> 提到上面。geometry_difftest.cpp 用 `namespace pkoracle`
// 解决同一件事，那边是为了跟 Qt 的同名符号分家，这里是为了跟干净 TU 分家。
//
// 附带的一条：PkGlobal.h 里 qIsNaN / qInf 是**声明在头、定义在 PkGlobal.cpp**
// 的非 inline 函数，落进匿名 namespace 后就没有定义了。本 TU 不用它们，所以没事；
// 抄这个形状的 TU 一旦用到，表现是响亮的链接错误，不是静默错行为。

#include "point_macro_proof.h"

#include <limits>

namespace {

// 破坏版：恒返回 false。真实的 pkFuzzyCompare/pkFuzzyIsNull 现在与 Qt 语义
// 一致（Task 1 已核对），拿它们当诱饵测不出东西，所以用一对必定不同的实现。
bool pkProofBrokenFuzzyCompare(double, double) { return false; }
bool pkProofBrokenFuzzyCompare(float, float) { return false; }
bool pkProofBrokenFuzzyIsNull(double) { return false; }
bool pkProofBrokenFuzzyIsNull(float) { return false; }

#define qFuzzyCompare pkProofBrokenFuzzyCompare
#define qFuzzyIsNull  pkProofBrokenFuzzyIsNull

// PkGlobal.h 检测到 qFuzzyCompare 已是宏时会整段让位，连 qAbs 也不再定义
//（那是给 pk/test 的垫片留的位置）。这里补上与 pk/test 那份逐字相同的 qAbs，
// 复现真实共存路径的形状。
template <typename T> constexpr T qAbs(const T &t) { return t >= T(0) ? t : -t; }

#include "../PkPoint.h"

} // 匿名 namespace —— 以上全部是本 TU 私有的内部链接实体

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
