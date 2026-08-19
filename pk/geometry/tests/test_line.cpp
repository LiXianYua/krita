#include "cases/line_case.h"
#include "../PkLine.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

// PkTestBinder<PkLineCase> 由 pk_test_moc.py 生成，像 Qt moc 输出一样直接
// #include 进本 TU（理由与 test_rect.cpp 相同）。
#include "pk_binder_line_case.inc"

// ---------------------------------------------------------------------------
// 期望值全部取自**真 Qt 5.15.7** 的实测输出（探针链
// /mnt/ssd-disk/liyang/projects/krita-ci-env/_install 的 libQt5Core，
// QT_VERSION_STR "5.15.7"）与一份独立差分脚本（不随交付物提交，见
// PkLine.cpp 顶部注释）对七个 out-of-line 成员的逐输入验证。对齐口径：
// 与 Qt 的任何行为差异默认都是缺陷，所以 Qt 那些反直觉的地方也一起钉住：
//   · **angle() 用 atan2(-dy,dx)**：Qt 的坐标系 y 朝下，所以"逆时针为正"这句
//     话按屏幕视觉看其实是顺时针——方向 (0,1)（视觉上的"下"）给 270°，
//     方向 (0,-1)（视觉上的"上"）给 90°。
//   · **isNull() 与 operator== 不是同一条公式**：isNull 是逐分量
//     qFuzzyCompare(x1,x2)&&qFuzzyCompare(y1,y2)；operator== 是两次
//     PkPointF::operator==（那条自带"任一侧为 0 就改走 fuzzyIsNull"的零分支）。
//   · **pointAt(t) 不夹持 t**：t<0/t>1 都是合法外插。
//   · **intersects() 的类型/交点用同一套 na/nb 参数化**，符号试错过——
//     叉积顺序 cross(c,b)/cross(c,a)，不是看着"对称"的其它排列。
// ---------------------------------------------------------------------------

namespace {

// **位精确**比较：`==` 会把 +0.0/-0.0 判等、把 NaN 判不等。
bool sameD(double a, double b)
{
    std::uint64_t ba, bb;
    std::memcpy(&ba, &a, sizeof ba);
    std::memcpy(&bb, &b, sizeof bb);
    if (ba == bb) return true;
    return (a != a) && (b != b);
}

// 三角函数结果的**相对误差**比较：本实现与真 Qt 的公式逐位相同（差分脚本
// 591 119 次扫描 mismatch=0），但单测这里不追求位相等——那是对拍的事
// （oracle/geometry_difftest.cpp 新增的 Line 一节），单测只钉住"公式对不对"。
bool closeD(double a, double b, double relTol = 1e-9)
{
    if (sameD(a, b)) return true;
    double diff = std::fabs(a - b);
    double scale = std::fmax(1.0, std::fmax(std::fabs(a), std::fabs(b)));
    return diff <= scale * relTol;
}

bool pointClose(const PkPointF &p, double x, double y)
{
    return closeD(p.x(), x) && closeD(p.y(), y);
}

} // namespace

// ═══ PkLine（int，最小面）═══════════════════════════════════════════════

void PkLineCase::lineDefaultAndFourCtors()
{
    // 真 Qt qline.h:56 的默认构造函数体是空的（不做任何初始化），但内部
    // PkPoint 的默认构造各把 xp/yp 置 0，效果上仍是 (0,0,0,0)。
    const PkLine d;
    PK_COMPARE(d.p1().x(), 0);
    PK_COMPARE(d.p1().y(), 0);
    PK_COMPARE(d.p2().x(), 0);
    PK_COMPARE(d.p2().y(), 0);

    const PkLine l1(1, 2, 3, 4);
    PK_COMPARE(l1.p1().x(), 1);
    PK_COMPARE(l1.p1().y(), 2);
    PK_COMPARE(l1.p2().x(), 3);
    PK_COMPARE(l1.p2().y(), 4);

    const PkLine l2(PkPoint(5, 6), PkPoint(7, 8));
    PK_COMPARE(l2.p1().x(), 5);
    PK_COMPARE(l2.p1().y(), 6);
    PK_COMPARE(l2.p2().x(), 7);
    PK_COMPARE(l2.p2().y(), 8);
}

void PkLineCase::lineImplicitPromotionToLineF()
{
    // 真实调用点：plugins/tools/tool_knife/RemoveGutterStrategy.cpp:56
    //   `QLineF l = QLine(QPoint(), QPoint(50, 50));`
    // 这是本 Task 唯一压到 PkLine 的真实形态：构造后立即隐式转 PkLineF。
    PkLineF l = PkLine(PkPoint(), PkPoint(50, 50));
    PK_COMPARE(l.p1().x(), 0.0);
    PK_COMPARE(l.p1().y(), 0.0);
    PK_COMPARE(l.p2().x(), 50.0);
    PK_COMPARE(l.p2().y(), 50.0);

    PK_VERIFY((std::is_convertible<PkLine, PkLineF>::value));
}

// ═══ PkLineF 构造与布局 ═══════════════════════════════════════════════════

void PkLineCase::lineFDefaultCtorIsAllZero()
{
    const PkLineF d;
    PK_VERIFY(sameD(d.p1().x(), 0.0) && sameD(d.p1().y(), 0.0));
    PK_VERIFY(sameD(d.p2().x(), 0.0) && sameD(d.p2().y(), 0.0));
    PK_VERIFY(d.isNull());
}

void PkLineCase::lineFFourConstructors()
{
    const PkLineF a(PkPointF(1, 2), PkPointF(3, 4));
    PK_COMPARE(a.x1(), 1.0); PK_COMPARE(a.y1(), 2.0);
    PK_COMPARE(a.x2(), 3.0); PK_COMPARE(a.y2(), 4.0);

    const PkLineF b(1.5, 2.5, 3.5, 4.5);
    PK_COMPARE(b.x1(), 1.5); PK_COMPARE(b.y1(), 2.5);
    PK_COMPARE(b.x2(), 3.5); PK_COMPARE(b.y2(), 4.5);

    // (const PkLine&) 隐式提升 —— 见 lineImplicitPromotionToLineF，这里只
    // 补一条直接构造的形态。
    const PkLineF c = PkLine(1, 2, 3, 4);
    PK_COMPARE(c.x1(), 1.0); PK_COMPARE(c.y2(), 4.0);
}

void PkLineCase::lineFLayoutIsFourQreal()
{
    // 实测真 Qt 5.15.7：sizeof(QLineF)==32（两个 QPointF，无虚表、无 padding）。
    PK_COMPARE(sizeof(PkLineF), sizeof(qreal) * 4);
    PK_VERIFY(std::is_trivially_copyable<PkLineF>::value);
    PK_VERIFY(std::is_standard_layout<PkLineF>::value);
}

// ═══ 取值器 ═══════════════════════════════════════════════════════════════

void PkLineCase::lineFAccessorsX1Y1X2Y2()
{
    const PkLineF l(-1.5, 2.5, 3.5, -4.5);
    PK_COMPARE(l.x1(), -1.5);
    PK_COMPARE(l.y1(), 2.5);
    PK_COMPARE(l.x2(), 3.5);
    PK_COMPARE(l.y2(), -4.5);
    PK_VERIFY(sameD(l.p1().x(), -1.5) && sameD(l.p1().y(), 2.5));
    PK_VERIFY(sameD(l.p2().x(), 3.5) && sameD(l.p2().y(), -4.5));
}

void PkLineCase::lineFDxDy()
{
    const PkLineF l(1, 2, 4, 8);
    PK_COMPARE(l.dx(), 3.0);
    PK_COMPARE(l.dy(), 6.0);
}

void PkLineCase::lineFIsNullIsFuzzy()
{
    // isNull() 是逐分量 qFuzzyCompare，**不是**逐位相等，**也不是"足够接近
    // 0 就算相等"**：qFuzzyCompare 的右端取 qMin(|a|,|b|)，任一侧恰好是 0
    // 时这一项恒为 0，于是比较**恒假**（唯一例外是两侧都恰好是 0）。真 Qt
    // 5.15.7 实测：`QLineF(0,0,1e-13,0).isNull()` 与
    // `QLineF(0,0,1e-11,0).isNull()` **都是 false**——起点在 0 时压根挤不进
    // "足够接近"这条路；只有两侧都远离 0（比如 1 与 1+1e-13）时相对阈值
    // 才起作用。这与 PkPointF::operator==/PkRectF::operator== 的零侧行为
    // 是同一条公式的同一个反直觉后果（README「Qt 语义里必须照抄」一节）。
    PK_VERIFY(PkLineF(0, 0, 0, 0).isNull());
    PK_VERIFY(PkLineF(1, 1, 1, 1).isNull());
    PK_VERIFY(!PkLineF(0, 0, 1, 0).isNull());
    PK_VERIFY(!PkLineF(0, 0, 1e-13, 0).isNull());
    PK_VERIFY(!PkLineF(0, 0, 1e-11, 0).isNull());
    // 两侧都远离 0 时相对阈值才生效：真 Qt 5.15.7 实测
    // `QLineF(1,1, 1+1e-13,1).isNull()` 为 true。
    PK_VERIFY(PkLineF(1, 1, 1 + 1e-13, 1).isNull());
}

// ═══ length / setLength ═══════════════════════════════════════════════════

void PkLineCase::lineFLength()
{
    PK_VERIFY(closeD(PkLineF(0, 0, 3, 4).length(), 5.0));
    PK_VERIFY(closeD(PkLineF(0, 0, 0, 0).length(), 0.0));
    PK_VERIFY(closeD(PkLineF(-3, -4, 0, 0).length(), 5.0));
}

void PkLineCase::lineFSetLength()
{
    // 真 Qt 5.15.7 实测：(0,0,3,4) 长度 5，setLength(10) 得 p2=(6,8)——
    // 按 dx/oldLength、dy/oldLength 的方向比例缩放，不是重新算角度。
    PkLineF l(0, 0, 3, 4);
    l.setLength(10);
    PK_VERIFY(pointClose(l.p2(), 6.0, 8.0));

    PkLineF neg(0, 0, 3, 4);
    neg.setLength(-10);
    PK_VERIFY(pointClose(neg.p2(), -6.0, -8.0));
}

void PkLineCase::lineFSetLengthOnNullLine()
{
    // 真 Qt 5.15.7 实测：oldLength<=0（含恰好 0）时 setLength 是**空操作**
    // ——p2 原样不变，不会因为除以 0 变成 NaN/inf。这是反直觉的一条，容易被
    // "顺手改成沿默认方向延伸"，照抄 Qt 的 `if (oldLength > 0)` 才对。
    PkLineF l(1, 1, 1, 1);
    l.setLength(10);
    PK_VERIFY(pointClose(l.p2(), 1.0, 1.0));
}

// ═══ angle / setAngle / angleTo ═══════════════════════════════════════════

void PkLineCase::lineFAngleCardinalDirections()
{
    // 真 Qt 5.15.7 实测（探针 LD_LIBRARY_PATH 链真 libQt5Core）：
    //   (0,0)->(1,0)  => 0（打印为 -0，与 0 相等）
    //   (0,0)->(0,1)  => 270（Qt 坐标系 y 朝下，"视觉向下"给 270°）
    //   (0,0)->(0,-1) => 90
    //   (0,0)->(-1,0) => 180
    //   (0,0)->(1,1)  => 315
    PK_VERIFY(closeD(PkLineF(0, 0, 1, 0).angle(), 0.0));
    PK_VERIFY(closeD(PkLineF(0, 0, 0, 1).angle(), 270.0));
    PK_VERIFY(closeD(PkLineF(0, 0, 0, -1).angle(), 90.0));
    PK_VERIFY(closeD(PkLineF(0, 0, -1, 0).angle(), 180.0));
    PK_VERIFY(closeD(PkLineF(0, 0, 1, 1).angle(), 315.0));
}

void PkLineCase::lineFAngleOfNullLineIsNegativeZero()
{
    // 真 Qt 5.15.7 实测：`QLineF(0,0,0,0).angle()` 打印为 -0（atan2(-0,0)==-0，
    // 归一化分支 `theta<0` 对 -0 为假，原样返回）。`-0.0 == 0.0`，
    // 所以 PK_COMPARE 这条断言对它免疫；用 std::signbit 单独钉一次符号位。
    const double a = PkLineF(0, 0, 0, 0).angle();
    PK_COMPARE(a, 0.0);
    PK_VERIFY(std::signbit(a));
}

void PkLineCase::lineFSetAngle()
{
    PkLineF l(0, 0, 5, 0);
    l.setAngle(90);
    // 真 Qt 5.15.7 实测：p2=(3.0616169978683831e-16, -5)——cos(90°) 不是
    // 精确 0（浮点弧度转换的余量），照抄这份"不精确"而不是特判 90°。
    PK_VERIFY(pointClose(l.p2(), 0.0, -5.0));
}

void PkLineCase::lineFAngleTo()
{
    // 真 Qt 5.15.7 实测：a=(0,0,1,0)[0°] 到 b=(0,0,0,1)[270°]：
    //   a.angleTo(b) == 270，b.angleTo(a) == 90（不可交换）。
    const PkLineF a(0, 0, 1, 0), b(0, 0, 0, 1);
    PK_VERIFY(closeD(a.angleTo(b), 270.0));
    PK_VERIFY(closeD(b.angleTo(a), 90.0));
}

void PkLineCase::lineFAngleToWithNullLineIsZero()
{
    const PkLineF a(0, 0, 1, 0), nullLine(5, 5, 5, 5);
    PK_COMPARE(a.angleTo(nullLine), 0.0);
    PK_COMPARE(nullLine.angleTo(a), 0.0);
}

// ═══ unitVector / normalVector ═══════════════════════════════════════════

void PkLineCase::lineFUnitVector()
{
    const PkLineF l(0, 0, 10, 0);
    const PkLineF u = l.unitVector();
    PK_VERIFY(pointClose(u.p1(), 0.0, 0.0));
    PK_VERIFY(pointClose(u.p2(), 1.0, 0.0));
}

void PkLineCase::lineFNormalVector()
{
    // qline.h —— normalVector 是 (dy,-dx) 旋 90°，是 inline 的（不需要
    // 除以长度，与 unitVector 不同这一条不是 out-of-line）。
    const PkLineF l(0, 0, 10, 0);
    const PkLineF n = l.normalVector();
    PK_VERIFY(pointClose(n.p1(), 0.0, 0.0));
    PK_VERIFY(pointClose(n.p2(), 0.0, -10.0));
}

// ═══ translate / translated ═══════════════════════════════════════════════

void PkLineCase::lineFTranslateAndTranslated()
{
    PkLineF l(1, 2, 3, 4);
    l.translate(10, 20);
    PK_VERIFY(pointClose(l.p1(), 11.0, 22.0));
    PK_VERIFY(pointClose(l.p2(), 13.0, 24.0));

    l.translate(PkPointF(-1, -2));
    PK_VERIFY(pointClose(l.p1(), 10.0, 20.0));

    const PkLineF base(1, 2, 3, 4);
    const PkLineF t1 = base.translated(1, 1);
    PK_VERIFY(pointClose(t1.p1(), 2.0, 3.0));
    const PkLineF t2 = base.translated(PkPointF(2, 2));
    PK_VERIFY(pointClose(t2.p1(), 3.0, 4.0));
}

// ═══ pointAt：不夹持 t ═════════════════════════════════════════════════════

void PkLineCase::lineFPointAtExtrapolates()
{
    const PkLineF l(0, 0, 10, 0);
    PK_VERIFY(pointClose(l.pointAt(0.5), 5.0, 0.0));
    // 真 Qt：t<0 / t>1 都是合法外插，不夹持。
    PK_VERIFY(pointClose(l.pointAt(-0.5), -5.0, 0.0));
    PK_VERIFY(pointClose(l.pointAt(1.5), 15.0, 0.0));
}

// ═══ center ═══════════════════════════════════════════════════════════════

void PkLineCase::lineFCenterIsMidpoint()
{
    // 真实调用点：libs/flake/text/KoSvgTextShapeLayoutFunc_inShape.cpp:154 等
    // ≥9 处（QLineF line; ... line.center()）——任务给的"完整方法面"清单没
    // 点这个名字，实测证伪按判据①补上。qline.h 的公式是**两个 0.5 乘法**，
    // 不是 `(p1+p2)/2`。
    const PkLineF l(0, 0, 10, 20);
    PK_VERIFY(pointClose(l.center(), 5.0, 10.0));
}

// ═══ setP1 / setP2 ══════════════════════════════════════════════════════

void PkLineCase::lineFSetP1SetP2()
{
    PkLineF l(0, 0, 0, 0);
    l.setP1(PkPointF(1, 2));
    l.setP2(PkPointF(3, 4));
    PK_VERIFY(pointClose(l.p1(), 1.0, 2.0));
    PK_VERIFY(pointClose(l.p2(), 3.0, 4.0));
}

// ═══ intersects ═══════════════════════════════════════════════════════════

void PkLineCase::lineFIntersectsBounded()
{
    // 真 Qt 5.15.7 实测：两条对角线在 (5,5) 处交叉，两段都真的覆盖交点。
    const PkLineF a(0, 0, 10, 10), b(0, 10, 10, 0);
    PkPointF ip;
    const PkLineF::IntersectType t = a.intersects(b, &ip);
    PK_VERIFY(t == PkLineF::BoundedIntersection);
    PK_VERIFY(pointClose(ip, 5.0, 5.0));
}

void PkLineCase::lineFIntersectsUnbounded()
{
    // 真 Qt 5.15.7 实测：两条线所在直线相交，但交点落在至少一段的延长线上。
    const PkLineF a(0, 0, 1, 0), b(5, 5, 10, 10);
    PkPointF ip;
    const PkLineF::IntersectType t = a.intersects(b, &ip);
    PK_VERIFY(t == PkLineF::UnboundedIntersection);
}

void PkLineCase::lineFIntersectsParallelIsNone()
{
    const PkLineF a(0, 0, 1, 0), b(5, 5, 6, 5);
    PkPointF ip;
    const PkLineF::IntersectType t = a.intersects(b, &ip);
    PK_VERIFY(t == PkLineF::NoIntersection);
}

void PkLineCase::lineFIntersectsAcceptsNullptr()
{
    const PkLineF a(0, 0, 10, 10), b(0, 10, 10, 0);
    const PkLineF::IntersectType t = a.intersects(b, nullptr);
    PK_VERIFY(t == PkLineF::BoundedIntersection);
}

// ═══ fromPolar ══════════════════════════════════════════════════════════

void PkLineCase::lineFFromPolar()
{
    // 真实调用点：libs/image/brushengine/kis_paintop_settings.cpp:558 等
    // 3 处——任务给的"完整方法面"清单没点这个名字，实测证伪按判据①补上。
    // 公式：PkLineF(0,0, len*cos(θ), -len*sin(θ))，θ 是角度转弧度。
    const PkLineF l = PkLineF::fromPolar(10, 0);
    PK_VERIFY(pointClose(l.p2(), 10.0, 0.0));
    const PkLineF l2 = PkLineF::fromPolar(10, 90);
    PK_VERIFY(pointClose(l2.p2(), 0.0, -10.0));
}

// ═══ operator== / != ═══════════════════════════════════════════════════

void PkLineCase::lineFEqualityIsFuzzy()
{
    // ⚠ operator== 走的是**两次 PkPointF::operator==**（各带零分支），
    // 不是逐分量 qFuzzyCompare（那是 isNull() 的公式，两条不是同一回事）。
    PK_VERIFY(PkLineF(1, 2, 3, 4) == PkLineF(1, 2, 3, 4));
    PK_VERIFY(PkLineF(1, 2, 3, 4) != PkLineF(1, 2, 3, 5));
    // 任一端点上"一侧为 0"的零分支：PkPointF(0,0)==PkPointF(1e-300,0) 为真。
    PK_VERIFY(PkLineF(0, 0, 1, 1) == PkLineF(1e-300, 0, 1, 1));
}

// ═══ 跨切面 ═══════════════════════════════════════════════════════════════

void PkLineCase::lineFNoexceptSurfaceMatchesQt()
{
    // ⚠ qline.h 全程没有一个 noexcept 标注（与 qpoint.h/qsize.h/qrect.h 都
    // 不同）。抽两个有代表性的成员钉住这条"没有"，别顺手给替代品加上。
    PkLineF l(0, 0, 1, 1);
    PK_VERIFY(!noexcept(PkLineF(0, 0, 1, 1)));
    PK_VERIFY(!noexcept(l.length()));
    PK_VERIFY(!noexcept(l.translate(1, 1)));
}

int run_line_tests()
{
    PkLineCase tc;
    const char *argv[] = {"test_pkgeometry"};
    return PkTest::qExec(&tc, 1, const_cast<char **>(argv));
}
