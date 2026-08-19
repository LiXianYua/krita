#include "cases/polygon_case.h"
#include "../PkPolygon.h"
#include "../PkTransform.h"

#include <cmath>
#include <cstring>
#include <cstdint>
#include <iterator>
#include <type_traits>

// PkTestBinder<PkPolygonCase> 由 pk_test_moc.py 生成，像 Qt moc 输出一样直接
// #include 进本 TU（理由与 test_line.cpp 相同）。
#include "pk_binder_polygon_case.inc"

// ---------------------------------------------------------------------------
// 期望值全部取自**真 Qt 5.15.7** 的实测输出（探针链
// /mnt/ssd-disk/liyang/projects/krita-ci-env/_install 的 libQt5Core/libQt5Gui，
// QT_VERSION_STR "5.15.7"）。对齐口径：与 Qt 的任何行为差异默认都是缺陷，
// 所以 Qt 那些反直觉的地方也一起钉住：
//   · **containsPoint 的自相交多边形**：经典五角星在中心点上 OddEvenFill 与
//     WindingFill 给出不同答案（false / true）——这正是这两种填充规则存在的
//     理由，不是巧合。
//   · **PkPolygonF(const PkRectF&) 是 5 个点，不是 4 个**：顺时针闭合回起点。
//   · **squareToQuad 的透视分支**：四点不构成平行四边形时走 8 参数透视解，
//     取值来自真 Qt 探针（手推这组数字容易在符号上出错）。
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

// ═══ PkPolygon（int，最小面）═══════════════════════════════════════════════

void PkPolygonCase::polygonDefaultCtorIsEmpty()
{
    const PkPolygon p;
    PK_VERIFY(p.isEmpty());
    PK_COMPARE(p.size(), 0);
}

void PkPolygonCase::polygonFromVectorCtorIsImplicit()
{
    // 真实调用点：kis_convex_hull.cpp:82 `QPolygon(points)`，points 是
    // `const QVector<QPoint>&`——非 explicit 是这条调用形态的前提。
    PK_VERIFY((std::is_convertible<PkVector<PkPoint>, PkPolygon>::value));

    PkVector<PkPoint> v;
    v << PkPoint(1, 2) << PkPoint(3, 4);
    const PkPolygon p(v);
    PK_COMPARE(p.size(), 2);
    PK_COMPARE(p.at(0).x(), 1);
    PK_COMPARE(p.at(1).y(), 4);
}

void PkPolygonCase::polygonInitializerListCtor()
{
    // 真实调用点：KisTofuGlyph.cpp 的 `QPolygon a{QVector<QPoint>{{x,y},...}}`
    // ——外层花括号落到 PkPolygon(const PkVector<PkPoint>&) 上，PkVector 自己
    // 的 initializer_list 构造把内层花括号接住。
    const PkPolygon p{PkVector<PkPoint>{PkPoint(1, 1), PkPoint(2, 2), PkPoint(3, 3)}};
    PK_COMPARE(p.size(), 3);
    PK_COMPARE(p.at(2).x(), 3);
}

void PkPolygonCase::polygonIteratorAndPushBackForBoost()
{
    // kis_convex_hull.cpp 的 boost::geometry 适配要求 QPolygon::iterator /
    // QPolygon::const_iterator 存在、begin()/end() 是标准迭代器接口、输出侧
    // 支持 push_back —— 全部继承自 PkVector<PkPoint>（PkArrayContainer），
    // 这里只钉住"继承确实带来了这些名字/行为"，不重新实现 boost 适配本身
    // （那部分在 graft/ 里用真实生产源文件验证）。
    PkPolygon p;
    p.push_back(PkPoint(1, 1));
    p.push_back(PkPoint(2, 2));
    PK_COMPARE(p.size(), 2);

    PkPolygon::iterator it = p.begin();
    PK_VERIFY(it != p.end());
    int sum = 0;
    for (PkPolygon::const_iterator cit = p.begin(); cit != p.end(); ++cit) {
        sum += cit->x();
    }
    PK_COMPARE(sum, 3);

    // std::distance 是 boost range 概念用得到的另一条标准迭代器保证。
    PK_COMPARE((int)std::distance(p.begin(), p.end()), 2);
}

// ═══ PkPolygonF 构造 ═══════════════════════════════════════════════════════

void PkPolygonCase::polygonFDefaultCtorIsEmpty()
{
    const PkPolygonF p;
    PK_VERIFY(p.isEmpty());
}

void PkPolygonCase::polygonFSizeCtorIsExplicit()
{
    PK_VERIFY(!(std::is_convertible<int, PkPolygonF>::value));
    const PkPolygonF p(3);
    PK_COMPARE(p.size(), 3);
}

void PkPolygonCase::polygonFFromVectorCtorIsImplicit()
{
    PK_VERIFY((std::is_convertible<PkVector<PkPointF>, PkPolygonF>::value));
    // 真实调用点：CutThroughShapeStrategy.cpp:186
    //   `QPolygonF({leftLine.p1(), leftLine.p2(), ...})`
    // ——直接给花括号列表当实参，落到 PkVector<PkPointF> 的 initializer_list
    // 构造再转 PkPolygonF，是同一条构造链。
    const PkPolygonF p({PkPointF(1, 1), PkPointF(2, 2), PkPointF(3, 3)});
    PK_COMPARE(p.size(), 3);
}

void PkPolygonCase::polygonFFromRectCtorIsFivePointsClockwise()
{
    // 真 Qt 5.15.7 实测：QPolygonF(QRectF(1,2,3,4)) 的五个点
    //   (1,2) (4,2) (4,6) (1,6) (1,2)  —— 顺时针，首尾闭合。
    const PkPolygonF p(PkRectF(1, 2, 3, 4));
    PK_COMPARE(p.size(), 5);
    PK_VERIFY(pointClose(p.at(0), 1.0, 2.0));
    PK_VERIFY(pointClose(p.at(1), 4.0, 2.0));
    PK_VERIFY(pointClose(p.at(2), 4.0, 6.0));
    PK_VERIFY(pointClose(p.at(3), 1.0, 6.0));
    PK_VERIFY(pointClose(p.at(4), 1.0, 2.0));
}

// ═══ translate / translated ═══════════════════════════════════════════════

void PkPolygonCase::polygonFTranslate()
{
    // 真 Qt 5.15.7 实测：QPolygonF(QRectF(0,0,1,1)) 的 [0] 平移 (5,5) 后
    // 得 (5,5)。
    PkPolygonF p(PkRectF(0, 0, 1, 1));
    p.translate(PkPointF(5, 5));
    PK_VERIFY(pointClose(p.at(0), 5.0, 5.0));

    // offset.isNull() 时是空操作（qpolygon.cpp 提前返回），钉住这条不触发的
    // 分支不会把点原地改写成别的值。
    PkPolygonF q(PkRectF(0, 0, 1, 1));
    q.translate(0.0, 0.0);
    PK_VERIFY(pointClose(q.at(2), 1.0, 1.0));
}

void PkPolygonCase::polygonFTranslated()
{
    // 真 Qt 5.15.7 实测：translate(5,5) 之后再 translated(1,1) 的 [2] 得 (7,7)。
    PkPolygonF p(PkRectF(0, 0, 1, 1));
    p.translate(PkPointF(5, 5));
    const PkPolygonF t = p.translated(1, 1);
    PK_VERIFY(pointClose(t.at(2), 7.0, 7.0));
    // translated 不改动原对象。
    PK_VERIFY(pointClose(p.at(2), 6.0, 6.0));
}

// ═══ isClosed ═══════════════════════════════════════════════════════════

void PkPolygonCase::polygonFIsClosed()
{
    // 真 Qt 5.15.7 实测：QPolygonF(QRectF(...)) 构造出来的多边形是闭合的
    // （首尾同点）；空多边形与手动追加、末点不等于首点的都不闭合。
    const PkPolygonF closed(PkRectF(1, 2, 3, 4));
    PK_VERIFY(closed.isClosed());

    const PkPolygonF empty;
    PK_VERIFY(!empty.isClosed());

    PkPolygonF open;
    open << PkPointF(0, 0) << PkPointF(1, 0) << PkPointF(1, 1);
    PK_VERIFY(!open.isClosed());
}

// ═══ boundingRect ═══════════════════════════════════════════════════════

void PkPolygonCase::polygonFBoundingRect()
{
    // 真 Qt 5.15.7 实测：QPolygonF(QRectF(1,2,3,4)).boundingRect() ==
    // QRectF(1,2,3,4) —— 往返自洽（这是 kis_algebra_2d_test.cpp:339 那条
    // QCOMPARE 断言的真实形态）。
    const PkPolygonF p(PkRectF(1, 2, 3, 4));
    const PkRectF br = p.boundingRect();
    PK_VERIFY(closeD(br.x(), 1.0) && closeD(br.y(), 2.0));
    PK_VERIFY(closeD(br.width(), 3.0) && closeD(br.height(), 4.0));
}

void PkPolygonCase::polygonFBoundingRectOfEmptyIsZero()
{
    // qpolygon.cpp:662-663 —— 空多边形显式返回 (0,0,0,0)。
    const PkPolygonF p;
    const PkRectF br = p.boundingRect();
    PK_VERIFY(closeD(br.x(), 0.0) && closeD(br.y(), 0.0));
    PK_VERIFY(closeD(br.width(), 0.0) && closeD(br.height(), 0.0));
}

// ═══ toPolygon ═══════════════════════════════════════════════════════════

void PkPolygonCase::polygonFToPolygonRounds()
{
    // 真 Qt 5.15.7 实测：(0.4,0.6) -> (0,1)，(-0.5,-0.5) -> (0,0)
    // ——PkPointF::toPoint() 的 qRound 语义（负半值向 +∞ 取整），toPolygon()
    // 只是逐点转发，不是重新发明取整规则。
    PkPolygonF fp;
    fp << PkPointF(0.4, 0.6) << PkPointF(-0.5, -0.5);
    const PkPolygon ip = fp.toPolygon();
    PK_COMPARE(ip.size(), 2);
    PK_COMPARE(ip.at(0).x(), 0);
    PK_COMPARE(ip.at(0).y(), 1);
    PK_COMPARE(ip.at(1).x(), 0);
    PK_COMPARE(ip.at(1).y(), 0);
}

// ═══ containsPoint：Qt::FillRule ═══════════════════════════════════════════

void PkPolygonCase::polygonFContainsPointSquareInsideOutsideVertex()
{
    // 真 Qt 5.15.7 实测：10x10 正方形，内部点/外部点/顶点各自的取值。
    PkPolygonF sq;
    sq << PkPointF(0, 0) << PkPointF(10, 0) << PkPointF(10, 10) << PkPointF(0, 10);
    PK_VERIFY(sq.containsPoint(PkPointF(5, 5), Qt::OddEvenFill));
    PK_VERIFY(!sq.containsPoint(PkPointF(15, 5), Qt::OddEvenFill));
    PK_VERIFY(sq.containsPoint(PkPointF(0, 0), Qt::OddEvenFill));
}

void PkPolygonCase::polygonFContainsPointEmptyIsFalse()
{
    const PkPolygonF empty;
    PK_VERIFY(!empty.containsPoint(PkPointF(0, 0), Qt::OddEvenFill));
    PK_VERIFY(!empty.containsPoint(PkPointF(0, 0), Qt::WindingFill));
}

void PkPolygonCase::polygonFContainsPointStarDistinguishesFillRule()
{
    // 经典自相交五角星（五个顶点按"隔一个连一个"的顺序排列，边在中心区域
    // 两两交叉）。真 Qt 5.15.7 实测：中心点 (0,0) 在 OddEvenFill 下是
    // **false**（射线穿越偶数次）、在 WindingFill 下是 **true**（环绕数
    // 非零）——这正是 Qt::FillRule 存在的理由：两种规则在自相交多边形上
    // 给出不同答案，凸多边形上永远看不出区别。
    PkPolygonF star;
    star << PkPointF(0, -10) << PkPointF(2.35, 3.24) << PkPointF(-9.51, -3.09)
         << PkPointF(9.51, -3.09) << PkPointF(-2.35, 3.24);
    PK_VERIFY(!star.containsPoint(PkPointF(0, 0), Qt::OddEvenFill));
    PK_VERIFY(star.containsPoint(PkPointF(0, 0), Qt::WindingFill));
}

// ═══ PkTransform::map(PkPolygonF) ═══════════════════════════════════════

void PkPolygonCase::transformMapPolygonFTranslateFastPath()
{
    // 真 Qt 5.15.7 实测：fromTranslate(3,4).map({(1,1),(2,2)}) ==
    // {(4,5),(5,6)} —— 走 t<=TxTranslate 的 translated() 快路径。
    const PkTransform t = PkTransform::fromTranslate(3, 4);
    PkPolygonF poly;
    poly << PkPointF(1, 1) << PkPointF(2, 2);
    const PkPolygonF mapped = t.map(poly);
    PK_COMPARE(mapped.size(), 2);
    PK_VERIFY(pointClose(mapped.at(0), 4.0, 5.0));
    PK_VERIFY(pointClose(mapped.at(1), 5.0, 6.0));
}

void PkPolygonCase::transformMapPolygonFRotateGeneralPath()
{
    // 真 Qt 5.15.7 实测：rotate(90) 是直角特判（TxRotate 档，精确值，不是
    // sin/cos 的浮点余量），map({(1,0),(0,1)}) == {(0,1),(-1,0)}。
    // 走的是 map(PkPolygonF) 的一般分支（t < TxProject，逐点复用
    // map(const PkPointF&)）。
    PkTransform t;
    t.rotate(90);
    PkPolygonF poly;
    poly << PkPointF(1, 0) << PkPointF(0, 1);
    const PkPolygonF mapped = t.map(poly);
    PK_VERIFY(pointClose(mapped.at(0), 0.0, 1.0));
    PK_VERIFY(pointClose(mapped.at(1), -1.0, 0.0));
}

// ═══ PkTransform::squareToQuad / quadToSquare ═══════════════════════════

void PkPolygonCase::transformSquareToQuadAffineParallelogram()
{
    // 平行四边形（ax==0 且 ay==0，走仿射分支，不需要透视）。真 Qt 5.15.7
    // 实测：quad=(0,0)(2,0)(3,2)(1,2) -> m11=2 m12=0 m21=1 m22=2 dx=dy=0
    // m13=m23=0；map(0,0)==quad[0]==(0,0)，map(1,1)==quad[2]==(3,2)
    // （单位正方形四角按顺序映到 quad 四角）。
    PkPolygonF quad;
    quad << PkPointF(0, 0) << PkPointF(2, 0) << PkPointF(3, 2) << PkPointF(1, 2);
    PkTransform t;
    const bool ok = PkTransform::squareToQuad(quad, t);
    PK_VERIFY(ok);
    PK_VERIFY(closeD(t.m11(), 2.0) && closeD(t.m12(), 0.0));
    PK_VERIFY(closeD(t.m21(), 1.0) && closeD(t.m22(), 2.0));
    PK_VERIFY(closeD(t.m13(), 0.0) && closeD(t.m23(), 0.0));
    PK_VERIFY(pointClose(t.map(PkPointF(0, 0)), 0.0, 0.0));
    PK_VERIFY(pointClose(t.map(PkPointF(1, 1)), 3.0, 2.0));
}

void PkPolygonCase::transformSquareToQuadPerspectiveTrapezoid()
{
    // 非平行四边形梯形（走透视分支，8 参数解）。真 Qt 5.15.7 实测：
    // quad=(0,0)(4,0)(3,2)(1,2) -> m11=4 m12=0 m13=-0 m21=2 m22=4 m23=1
    // dx=dy=0 m33=1；单位正方形四角精确映到 quad 四角（squareToQuad 的
    // 定义就是这个往返，逐点核对比只核对九个分量更贴近"这条变换真的对不对"）。
    PkPolygonF quad;
    quad << PkPointF(0, 0) << PkPointF(4, 0) << PkPointF(3, 2) << PkPointF(1, 2);
    PkTransform t;
    const bool ok = PkTransform::squareToQuad(quad, t);
    PK_VERIFY(ok);
    PK_VERIFY(closeD(t.m11(), 4.0) && closeD(t.m21(), 2.0) && closeD(t.m22(), 4.0));
    PK_VERIFY(closeD(t.m23(), 1.0));
    PK_VERIFY(pointClose(t.map(PkPointF(0, 0)), 0.0, 0.0));
    PK_VERIFY(pointClose(t.map(PkPointF(1, 0)), 4.0, 0.0));
    PK_VERIFY(pointClose(t.map(PkPointF(1, 1)), 3.0, 2.0));
    PK_VERIFY(pointClose(t.map(PkPointF(0, 1)), 1.0, 2.0));
}

void PkPolygonCase::transformSquareToQuadWrongCountFails()
{
    // qtransform.cpp:1812-1813 —— quad.count() != 4 直接 false，result 不动。
    PkPolygonF bad;
    bad << PkPointF(0, 0) << PkPointF(1, 0) << PkPointF(1, 1);
    PkTransform t;
    PK_VERIFY(!PkTransform::squareToQuad(bad, t));
}

void PkPolygonCase::transformQuadToSquareRoundTrips()
{
    // 真 Qt 5.15.7 实测：quadToSquare 是 squareToQuad 的逆——quad 四点各自
    // 经 quadToSquare 的结果矩阵映射，恰好落回单位正方形四角。
    PkPolygonF quad;
    quad << PkPointF(0, 0) << PkPointF(4, 0) << PkPointF(3, 2) << PkPointF(1, 2);
    PkTransform t;
    const bool ok = PkTransform::quadToSquare(quad, t);
    PK_VERIFY(ok);
    PK_VERIFY(pointClose(t.map(quad.at(0)), 0.0, 0.0));
    PK_VERIFY(pointClose(t.map(quad.at(1)), 1.0, 0.0));
    PK_VERIFY(pointClose(t.map(quad.at(2)), 1.0, 1.0));
    PK_VERIFY(pointClose(t.map(quad.at(3)), 0.0, 1.0));
}

int run_polygon_tests()
{
    PkPolygonCase tc;
    const char *argv[] = {"test_pkgeometry"};
    return PkTest::qExec(&tc, 1, const_cast<char **>(argv));
}
