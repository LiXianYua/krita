#include "cases/margins_case.h"
#include "../PkMargins.h"
#include "../PkRect.h"

#include <cstdint>
#include <cstring>
#include <type_traits>

// PkTestBinder<PkMarginsCase> 由 pk_test_moc.py 生成，像 Qt moc 输出一样直接
// #include 进本 TU（理由与 test_rect.cpp 相同）。
#include "pk_binder_margins_case.inc"

// ---------------------------------------------------------------------------
// 期望值取自真 Qt 5.15.7 的 include/QtCore/qmargins.h 全文（本机装的
// krita-ci-env Qt 前缀里就有这份头文件，两个类**全部是 inline 的**，
// 不需要对拍逆向——逐字抄就是权威来源）与探针实测（QRect::marginsAdded /
// QRectF::marginsAdded 的签名差异）。
//
// ⚠ **对 R-21 plan.md 的一条实测纠正**：plan.md 说 PkMargins 要
// "operator|（取分量最大值）"——真 Qt 5.15.7 的 QMargins **没有**这个运算符
// （逐字读过头文件确认，探针 `QMargins(1,2,3,4) | QMargins(...)` 编译失败：
// no match for operator|）。本文件不测一个不存在的运算符。
// ---------------------------------------------------------------------------

namespace {

bool sameD(double a, double b)
{
    std::uint64_t ba, bb;
    std::memcpy(&ba, &a, sizeof ba);
    std::memcpy(&bb, &b, sizeof bb);
    if (ba == bb) return true;
    return (a != a) && (b != b);
}

bool marginsAre(const PkMargins &m, int l, int t, int r, int b)
{
    return m.left() == l && m.top() == t && m.right() == r && m.bottom() == b;
}

bool marginsFAre(const PkMarginsF &m, double l, double t, double r, double b)
{
    return sameD(m.left(), l) && sameD(m.top(), t) && sameD(m.right(), r) && sameD(m.bottom(), b);
}

bool rectCoordsAre(const PkRect &r, int x1, int y1, int x2, int y2)
{
    return r.left() == x1 && r.top() == y1 && r.right() == x2 && r.bottom() == y2;
}

bool rectFFieldsAre(const PkRectF &r, double x, double y, double w, double h)
{
    return sameD(r.x(), x) && sameD(r.y(), y) && sameD(r.width(), w) && sameD(r.height(), h);
}

} // namespace

// ═══ PkMargins 构造与取值 ═══════════════════════════════════════════════

void PkMarginsCase::marginsDefaultCtorIsZero()
{
    const PkMargins m;
    PK_VERIFY(marginsAre(m, 0, 0, 0, 0));
    PK_VERIFY(m.isNull());
}

void PkMarginsCase::marginsFourArgCtor()
{
    const PkMargins m(1, 2, 3, 4);
    PK_VERIFY(marginsAre(m, 1, 2, 3, 4));
}

void PkMarginsCase::marginsSetters()
{
    PkMargins m;
    m.setLeft(1);
    m.setTop(2);
    m.setRight(3);
    m.setBottom(4);
    PK_VERIFY(marginsAre(m, 1, 2, 3, 4));
}

void PkMarginsCase::marginsIsNull()
{
    PK_VERIFY(PkMargins().isNull());
    PK_VERIFY(PkMargins(0, 0, 0, 0).isNull());
    PK_VERIFY(!PkMargins(1, 0, 0, 0).isNull());
    PK_VERIFY(!PkMargins(0, 0, 0, -1).isNull());
}

void PkMarginsCase::marginsEquality()
{
    PK_VERIFY(PkMargins(1, 2, 3, 4) == PkMargins(1, 2, 3, 4));
    PK_VERIFY(PkMargins(1, 2, 3, 4) != PkMargins(1, 2, 3, 5));
}

void PkMarginsCase::marginsLayoutIsFourInt()
{
    // 实测真 Qt 5.15.7：sizeof(QMargins)==16。
    PK_COMPARE(sizeof(PkMargins), sizeof(int) * 4);
    PK_VERIFY(std::is_trivially_copyable<PkMargins>::value);
    PK_VERIFY(std::is_standard_layout<PkMargins>::value);
}

// ═══ PkMargins 算术 ═══════════════════════════════════════════════════════

void PkMarginsCase::marginsAddSubMargins()
{
    PK_VERIFY(marginsAre(PkMargins(1, 2, 3, 4) + PkMargins(10, 10, 10, 10), 11, 12, 13, 14));
    PK_VERIFY(marginsAre(PkMargins(11, 12, 13, 14) - PkMargins(10, 10, 10, 10), 1, 2, 3, 4));

    PkMargins m(1, 2, 3, 4);
    m += PkMargins(1, 1, 1, 1);
    PK_VERIFY(marginsAre(m, 2, 3, 4, 5));
    m -= PkMargins(1, 1, 1, 1);
    PK_VERIFY(marginsAre(m, 1, 2, 3, 4));
}

void PkMarginsCase::marginsAddSubInt()
{
    PK_VERIFY(marginsAre(PkMargins(1, 2, 3, 4) + 5, 6, 7, 8, 9));
    PK_VERIFY(marginsAre(5 + PkMargins(1, 2, 3, 4), 6, 7, 8, 9));
    PK_VERIFY(marginsAre(PkMargins(6, 7, 8, 9) - 5, 1, 2, 3, 4));

    PkMargins m(1, 2, 3, 4);
    m += 5;
    PK_VERIFY(marginsAre(m, 6, 7, 8, 9));
    m -= 5;
    PK_VERIFY(marginsAre(m, 1, 2, 3, 4));
}

void PkMarginsCase::marginsMulDivInt()
{
    PK_VERIFY(marginsAre(PkMargins(1, 2, 3, 4) * 2, 2, 4, 6, 8));
    PK_VERIFY(marginsAre(2 * PkMargins(1, 2, 3, 4), 2, 4, 6, 8));
    PK_VERIFY(marginsAre(PkMargins(4, 4, 4, 4) / 2, 2, 2, 2, 2));

    PkMargins m(1, 2, 3, 4);
    m *= 2;
    PK_VERIFY(marginsAre(m, 2, 4, 6, 8));
    m /= 2;
    PK_VERIFY(marginsAre(m, 1, 2, 3, 4));
}

void PkMarginsCase::marginsMulDivQrealRounds()
{
    // qmargins.h —— 标量是 qreal 时结果**按 qRound 取整**（int 分量装不下
    // 浮点），与 int 版直接乘不同。真 Qt 5.15.7 实测
    // `QMargins(1,2,3,4) * 1.5` 得 (2,3,5,6)（qRound(1.5)=2, qRound(3)=3,
    // qRound(4.5)=5——qRound 对半值向 +∞ 取整、qRound(6)=6）。
    PK_VERIFY(marginsAre(PkMargins(1, 2, 3, 4) * 1.5, 2, 3, 5, 6));
    PK_VERIFY(marginsAre(1.5 * PkMargins(1, 2, 3, 4), 2, 3, 5, 6));

    PkMargins m(1, 2, 3, 4);
    m *= 1.5;
    PK_VERIFY(marginsAre(m, 2, 3, 5, 6));
}

void PkMarginsCase::marginsUnaryPlusMinus()
{
    PK_VERIFY(marginsAre(+PkMargins(1, 2, 3, 4), 1, 2, 3, 4));
    PK_VERIFY(marginsAre(-PkMargins(1, 2, 3, 4), -1, -2, -3, -4));
}

// ═══ PkMarginsF ═══════════════════════════════════════════════════════════

void PkMarginsCase::marginsFDefaultAndCtor()
{
    PK_VERIFY(PkMarginsF().isNull());
    const PkMarginsF m(1.5, 2.5, 3.5, 4.5);
    PK_VERIFY(marginsFAre(m, 1.5, 2.5, 3.5, 4.5));
}

void PkMarginsCase::marginsFPromotionFromMarginsIsImplicit()
{
    // qmargins.h —— **非 explicit**：PkMargins → PkMarginsF 隐式提升，
    // PkRectF::marginsAdded(PkMargins(...)) 这类调用形态靠它。
    const PkMarginsF f = PkMargins(1, 2, 3, 4);
    PK_VERIFY(marginsFAre(f, 1.0, 2.0, 3.0, 4.0));
    PK_VERIFY((std::is_convertible<PkMargins, PkMarginsF>::value));
    PK_VERIFY(!(std::is_convertible<PkMarginsF, PkMargins>::value));
}

void PkMarginsCase::marginsFIsNullIsFuzzy()
{
    // isNull() 逐分量 pkQtFuzzyIsNull（阈值 1e-12），不是精确 ==0。
    PK_VERIFY(PkMarginsF().isNull());
    PK_VERIFY(PkMarginsF(1e-13, 0, 0, 0).isNull());
    PK_VERIFY(!PkMarginsF(1e-11, 0, 0, 0).isNull());
}

void PkMarginsCase::marginsFEqualityIsFuzzy()
{
    PK_VERIFY(PkMarginsF(1, 2, 3, 4) == PkMarginsF(1, 2, 3, 4));
    // 相对阈值内的差异仍判等（pkQtFuzzyCompare，与 PkPointF 等一族同源）。
    PK_VERIFY(PkMarginsF(1, 2, 3, 4) == PkMarginsF(1 + 1e-13, 2, 3, 4));
    PK_VERIFY(PkMarginsF(1, 2, 3, 4) != PkMarginsF(1.5, 2, 3, 4));
}

void PkMarginsCase::marginsFArithmetic()
{
    PK_VERIFY(marginsFAre(PkMarginsF(1, 2, 3, 4) + PkMarginsF(1, 1, 1, 1), 2, 3, 4, 5));
    PK_VERIFY(marginsFAre(PkMarginsF(1, 2, 3, 4) - PkMarginsF(1, 1, 1, 1), 0, 1, 2, 3));
    PK_VERIFY(marginsFAre(PkMarginsF(1, 2, 3, 4) * 2.0, 2, 4, 6, 8));
    PK_VERIFY(marginsFAre(PkMarginsF(4, 4, 4, 4) / 2.0, 2, 2, 2, 2));

    PkMarginsF m(1, 2, 3, 4);
    m += 1.0;
    PK_VERIFY(marginsFAre(m, 2, 3, 4, 5));
    m -= 1.0;
    PK_VERIFY(marginsFAre(m, 1, 2, 3, 4));
}

void PkMarginsCase::marginsFToMarginsRounds()
{
    const PkMargins m = PkMarginsF(1.5, 2.4, 2.5, -1.5).toMargins();
    // qRound 对半值向 +∞ 取整：1.5->2, 2.4->2, 2.5->3, -1.5->-1。
    PK_VERIFY(marginsAre(m, 2, 2, 3, -1));
}

// ═══ 与 PkRect / PkRectF 的互操作 ══════════════════════════════════════════

void PkMarginsCase::rectMarginsAddedAndRemoved()
{
    // 真 Qt 5.15.7 实测：QRect(0,0,10,10).marginsAdded(QMargins(1,2,3,4))
    // 的内部坐标是 (-1,-2,12,13)。
    const PkRect r(0, 0, 10, 10);
    const PkRect added = r.marginsAdded(PkMargins(1, 2, 3, 4));
    PK_VERIFY(rectCoordsAre(added, -1, -2, 12, 13));

    // 实测 QRect(0,0,10,10).marginsRemoved(QMargins(1,2,3,4)) 内部坐标
    // (1,2,6,5)。
    const PkRect removed = r.marginsRemoved(PkMargins(1, 2, 3, 4));
    PK_VERIFY(rectCoordsAre(removed, 1, 2, 6, 5));

    // ⚠ 往返**不是**恒等映射的字面意思"坐标退回构造实参"：`PkRect(0,0,10,10)`
    // 的内部坐标本来就是 (0,0,9,9)（宽 10 = 跨距+1），marginsAdded 再
    // marginsRemoved 走的是同一条 +/-，退回来的自然还是内部坐标 (0,0,9,9)。
    // 真 Qt 5.15.7 实测确认（探针 `r.marginsAdded(...).marginsRemoved(...)`）。
    PkRect r2(0, 0, 10, 10);
    r2 += PkMargins(1, 2, 3, 4);
    PK_VERIFY(rectCoordsAre(r2, -1, -2, 12, 13));
    r2 -= PkMargins(1, 2, 3, 4);
    PK_VERIFY(rectCoordsAre(r2, 0, 0, 9, 9));
}

void PkMarginsCase::rectOperatorPlusMinusMargins()
{
    // 实测 QRect(0,0,10,10) + QMargins(1,1,1,1) 内部坐标 (-1,-1,10,10)；
    // QRect(0,0,10,10) - QMargins(1,1,1,1) 内部坐标 (1,1,8,8)。
    const PkRect r(0, 0, 10, 10);
    PK_VERIFY(rectCoordsAre(r + PkMargins(1, 1, 1, 1), -1, -1, 10, 10));
    PK_VERIFY(rectCoordsAre(PkMargins(1, 1, 1, 1) + r, -1, -1, 10, 10));
    PK_VERIFY(rectCoordsAre(r - PkMargins(1, 1, 1, 1), 1, 1, 8, 8));
}

void PkMarginsCase::rectFMarginsAddedAndRemoved()
{
    // 真 Qt 5.15.7 实测：QRectF(0,0,10,10).marginsAdded(QMarginsF(1,2,3,4))
    // 得 x=-1 y=-2 w=14 h=16（宽高各按左右/上下两侧之和加）。
    const PkRectF r(0, 0, 10, 10);
    const PkRectF added = r.marginsAdded(PkMarginsF(1, 2, 3, 4));
    PK_VERIFY(rectFFieldsAre(added, -1, -2, 14, 16));

    PkRectF r2(0, 0, 10, 10);
    r2 += PkMarginsF(1, 2, 3, 4);
    PK_VERIFY(rectFFieldsAre(r2, -1, -2, 14, 16));
    r2 -= PkMarginsF(1, 2, 3, 4);
    PK_VERIFY(rectFFieldsAre(r2, 0, 0, 10, 10));
}

void PkMarginsCase::rectFOperatorPlusMinusMarginsF()
{
    const PkRectF r(0, 0, 10, 10);
    const PkRectF added = r + PkMarginsF(1, 2, 3, 4);
    PK_VERIFY(rectFFieldsAre(added, -1, -2, 14, 16));
    const PkRectF added2 = PkMarginsF(1, 2, 3, 4) + r;
    PK_VERIFY(rectFFieldsAre(added2, -1, -2, 14, 16));
    const PkRectF removed = r.marginsAdded(PkMarginsF(1, 2, 3, 4)) - PkMarginsF(1, 2, 3, 4);
    PK_VERIFY(rectFFieldsAre(removed, 0, 0, 10, 10));
}

void PkMarginsCase::rectFMarginsAddedAcceptsMarginsPromotion()
{
    // ⚠ **签名吃 PkMarginsF，不是 PkMargins**——探针实测确认。这里钉住
    // "传入 PkMargins 仍能编过"这条，靠的是 PkMargins→PkMarginsF 的隐式
    // 提升，不是 PkRectF::marginsAdded 另有一个吃 PkMargins 的重载。
    const PkRectF r(0, 0, 10, 10);
    const PkRectF added = r.marginsAdded(PkMargins(1, 2, 3, 4));
    PK_VERIFY(rectFFieldsAre(added, -1, -2, 14, 16));
}

int run_margins_tests()
{
    PkMarginsCase tc;
    const char *argv[] = {"test_pkgeometry"};
    return PkTest::qExec(&tc, 1, const_cast<char **>(argv));
}
