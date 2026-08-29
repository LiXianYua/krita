#include "cases/painterpath_case.h"
#include "../PkPainterPath.h"

#include <cmath>
#include <cfloat>

// ⚠ 这个系统头必须在 oracle/geometry_difftest.cpp 顶部的系统头区里也出现过
// —— 理由与 PkLine.cpp 顶部同一条纪律。
#include <type_traits>

// PkTestBinder<PkPainterPathCase> 特化由 pk_test_moc.py 生成。
#include "pk_binder_painterpath_case.inc"

namespace {

// 两个浮点数的绝对值差异（用于非精确对比）。
constexpr qreal kEpsilon = 1e-9;

// 三次曲线 Kappa 常数（addEllipse 用）。
// 这是 Qt 的 QPainterPath::addEllipse 内部使用的常数。
// 来源：qpainterpath.cpp 的 addEllipse 实现。
// 值 = 4*(sqrt(2)-1)/3 ≈ 0.55228474983079339840
static constexpr qreal kappa = 0.55228474983079339840;

} // namespace

// ============================================================================
// 构造与基本状态
// ============================================================================

void PkPainterPathCase::defaultCtor()
{
    PkPainterPath path;
    PK_VERIFY(path.isEmpty());
    PK_COMPARE(path.elementCount(), 0);
    PK_COMPARE(path.fillRule(), Qt::OddEvenFill);
    PK_COMPARE(path.boundingRect(), PkRectF(0, 0, 0, 0));
}

void PkPainterPathCase::startPointCtor()
{
    PkPainterPath path(PkPointF(10, 20));
    PK_VERIFY(path.isEmpty());
    PK_COMPARE(path.elementCount(), 1);
    PK_COMPARE(path.currentPosition(), PkPointF(10, 20));
    PK_VERIFY(path.elementAt(0).isMoveTo());
    PK_COMPARE(path.elementAt(0).x, 10.0);
    PK_COMPARE(path.elementAt(0).y, 20.0);
}

void PkPainterPathCase::copyAndAssignment()
{
    PkPainterPath path(PkPointF(1, 2));
    path.lineTo(PkPointF(3, 4));
    path.setFillRule(Qt::WindingFill);

    // 拷贝构造
    PkPainterPath copy(path);
    PK_VERIFY(copy == path);
    PK_COMPARE(copy.fillRule(), Qt::WindingFill);

    // 拷贝赋值
    PkPainterPath assigned;
    assigned = path;
    PK_VERIFY(assigned == path);

    // 修改副本不影响原路径
    copy.lineTo(PkPointF(5, 6));
    PK_COMPARE(path.elementCount(), 2);
    PK_COMPARE(copy.elementCount(), 3);
}

// ============================================================================
// Element 类型
// ============================================================================

void PkPainterPathCase::elementTypeEnum()
{
    PK_COMPARE(static_cast<int>(PkPainterPath::MoveToElement), 0);
    PK_COMPARE(static_cast<int>(PkPainterPath::LineToElement), 1);
    PK_COMPARE(static_cast<int>(PkPainterPath::CurveToElement), 2);
    PK_COMPARE(static_cast<int>(PkPainterPath::CurveToDataElement), 3);
}

void PkPainterPathCase::elementAccessors()
{
    // Element 构造与访问
    PkPainterPath::Element e(3.5, 7.2, PkPainterPath::LineToElement);
    PK_COMPARE(e.x, 3.5);
    PK_COMPARE(e.y, 7.2);
    PK_COMPARE(e.type, PkPainterPath::LineToElement);

    // isMoveTo / isLineTo / isCurveTo
    PkPainterPath::Element m(1, 2, PkPainterPath::MoveToElement);
    PK_VERIFY(m.isMoveTo());
    PK_VERIFY(!m.isLineTo());
    PK_VERIFY(!m.isCurveTo());

    PkPainterPath::Element l(3, 4, PkPainterPath::LineToElement);
    PK_VERIFY(!l.isMoveTo());
    PK_VERIFY(l.isLineTo());
    PK_VERIFY(!l.isCurveTo());

    PkPainterPath::Element c(5, 6, PkPainterPath::CurveToElement);
    PK_VERIFY(!c.isMoveTo());
    PK_VERIFY(!c.isLineTo());
    PK_VERIFY(c.isCurveTo());

    PkPainterPath::Element cd(7, 8, PkPainterPath::CurveToDataElement);
    PK_VERIFY(!cd.isMoveTo());
    PK_VERIFY(!cd.isLineTo());
    PK_VERIFY(!cd.isCurveTo());

    // 隐式 operator PkPointF()
    PkPointF pt = e;
    PK_COMPARE(pt.x(), 3.5);
    PK_COMPARE(pt.y(), 7.2);

    // operator== / !=
    PkPainterPath::Element e1(1, 2, PkPainterPath::MoveToElement);
    PkPainterPath::Element e2(1, 2, PkPainterPath::MoveToElement);
    PkPainterPath::Element e3(1, 3, PkPainterPath::MoveToElement);
    PK_VERIFY(e1 == e2);
    PK_VERIFY(e1 != e3);
}

// ============================================================================
// 构建核心
// ============================================================================

void PkPainterPathCase::moveTo()
{
    PkPainterPath path;
    path.moveTo(PkPointF(10, 20));
    PK_COMPARE(path.elementCount(), 1);
    PK_VERIFY(path.elementAt(0).isMoveTo());
    PK_COMPARE(path.currentPosition(), PkPointF(10, 20));

    // 连续 moveTo 更新当前位置
    path.moveTo(30, 40);
    PK_COMPARE(path.elementCount(), 2);
    PK_VERIFY(path.elementAt(0).isMoveTo());
    PK_VERIFY(path.elementAt(1).isMoveTo());
    PK_COMPARE(path.currentPosition(), PkPointF(30, 40));
}

void PkPainterPathCase::lineTo()
{
    PkPainterPath path;
    path.moveTo(0, 0);
    path.lineTo(PkPointF(100, 100));
    PK_COMPARE(path.elementCount(), 2);
    PK_VERIFY(path.elementAt(1).isLineTo());
    PK_COMPARE(path.currentPosition(), PkPointF(100, 100));
}

void PkPainterPathCase::cubicTo()
{
    PkPainterPath path;
    path.moveTo(0, 0);
    path.cubicTo(PkPointF(10, 0), PkPointF(20, 0), PkPointF(30, 0));
    PK_COMPARE(path.elementCount(), 4); // 1 moveTo + 1 CurveToElement + 2 CurveToDataElement
    PK_VERIFY(path.elementAt(0).isMoveTo());
    PK_VERIFY(path.elementAt(1).isCurveTo());   // CurveToElement
    PK_VERIFY(!path.elementAt(2).isCurveTo());   // CurveToDataElement, not CurveTo
    PK_VERIFY(!path.elementAt(3).isCurveTo());   // CurveToDataElement, not CurveTo
    PK_COMPARE(path.currentPosition(), PkPointF(30, 0));

    // qreal 重载
    PkPainterPath path2;
    path2.moveTo(0, 0);
    path2.cubicTo(1, 1, 2, 2, 3, 3);
    PK_COMPARE(path2.elementCount(), 4);
    PK_COMPARE(path2.currentPosition(), PkPointF(3, 3));
}

void PkPainterPathCase::quadTo()
{
    PkPainterPath path;
    path.moveTo(0, 0);
    path.quadTo(PkPointF(50, 50), PkPointF(100, 0));
    // quadTo → cubicTo 转换：产生 1 moveTo + 1 CurveTo + 2 CurveToData
    PK_COMPARE(path.elementCount(), 4);
    PK_VERIFY(path.elementAt(0).isMoveTo());
    PK_VERIFY(path.elementAt(1).isCurveTo());
    PK_COMPARE(path.currentPosition(), PkPointF(100, 0));
}

void PkPainterPathCase::closeSubpath()
{
    PkPainterPath path;
    path.moveTo(0, 0);
    path.lineTo(100, 0);
    path.lineTo(100, 100);
    PK_COMPARE(path.elementCount(), 3);
    PK_VERIFY(!path.isClosed());

    path.closeSubpath();
    // closeSubpath 添加一条 lineTo 回到起点
    PK_COMPARE(path.elementCount(), 4);
    PK_VERIFY(path.elementAt(3).isLineTo());
    PK_COMPARE(path.elementAt(3).x, 0.0);
    PK_COMPARE(path.elementAt(3).y, 0.0);
    PK_VERIFY(path.isClosed());

    // 空路径上 closeSubpath 不做任何事
    PkPainterPath empty;
    empty.closeSubpath();
    PK_VERIFY(empty.isEmpty());
}

void PkPainterPathCase::currentPosition()
{
    PkPainterPath path;
    // 空路径的当前位置是 (0,0)
    PK_COMPARE(path.currentPosition(), PkPointF(0, 0));

    path.moveTo(10, 20);
    PK_COMPARE(path.currentPosition(), PkPointF(10, 20));

    path.lineTo(30, 40);
    PK_COMPARE(path.currentPosition(), PkPointF(30, 40));

    path.closeSubpath();
    // closeSubpath 后当前位置回到起点
    PK_COMPARE(path.currentPosition(), PkPointF(10, 20));
}

// ============================================================================
// 清理与预留
// ============================================================================

void PkPainterPathCase::clearAndReserve()
{
    PkPainterPath path(PkPointF(10, 10));
    path.lineTo(PkPointF(20, 20));
    path.setFillRule(Qt::WindingFill);

    path.clear();
    PK_VERIFY(path.isEmpty());
    PK_COMPARE(path.elementCount(), 0);
    PK_COMPARE(path.fillRule(), Qt::WindingFill); // fillRule 在 clear 后保留
    PK_COMPARE(path.currentPosition(), PkPointF(0, 0));

    // reserve 不改变大小
    PkPainterPath path2;
    path2.reserve(100);
    PK_VERIFY(path2.isEmpty());
}

// ============================================================================
// 查询
// ============================================================================

void PkPainterPathCase::isEmpty()
{
    PkPainterPath empty;
    PK_VERIFY(empty.isEmpty());

    PkPainterPath path(PkPointF(1, 2));
    PK_VERIFY(path.isEmpty());
    PK_COMPARE(path.elementCount(), 1);

    path.clear();
    PK_VERIFY(path.isEmpty());
}

void PkPainterPathCase::boundingRect()
{
    PkPainterPath empty;
    PK_COMPARE(empty.boundingRect(), PkRectF(0, 0, 0, 0));

    PkPainterPath path;
    path.moveTo(10, 20);
    PK_COMPARE(path.boundingRect(), PkRectF(10, 20, 0, 0));

    path.lineTo(200, 100);
    // 包围盒包含所有点
    PK_COMPARE(path.boundingRect(), PkRectF(10, 20, 190, 80));

    // 路径包含控制点（cubicTo 的控制点会扩展包围盒）
    PkPainterPath cubicPath;
    cubicPath.moveTo(0, 0);
    cubicPath.cubicTo(200, 0, 0, 200, 100, 100);
    // 包围盒包含控制点 (200,0) 和 (0,200)
    PkRectF bounds = cubicPath.boundingRect();
    PK_VERIFY(bounds.x() <= 0);
    PK_VERIFY(bounds.y() <= 0);
    PK_VERIFY(bounds.x() + bounds.width() >= 200);
    PK_VERIFY(bounds.y() + bounds.height() >= 200);
}

void PkPainterPathCase::controlPointRect()
{
    // 对于简单的 moveTo/lineTo，controlPointRect 与 boundingRect 相同
    PkPainterPath path;
    path.moveTo(10, 20);
    path.lineTo(200, 100);
    PK_COMPARE(path.controlPointRect(), path.boundingRect());
}

void PkPainterPathCase::isClosed()
{
    PkPainterPath empty;
    PK_VERIFY(!empty.isClosed());

    PkPainterPath openPath;
    openPath.moveTo(0, 0);
    openPath.lineTo(100, 100);
    PK_VERIFY(!openPath.isClosed());

    PkPainterPath closedPath;
    closedPath.moveTo(0, 0);
    closedPath.lineTo(100, 0);
    closedPath.lineTo(100, 100);
    closedPath.closeSubpath();
    PK_VERIFY(closedPath.isClosed());

    // addRect 产生闭合路径
    PkPainterPath rectPath;
    rectPath.addRect(PkRectF(0, 0, 50, 50));
    PK_VERIFY(rectPath.isClosed());
}

void PkPainterPathCase::elementCountAndAt()
{
    PkPainterPath path;
    path.moveTo(1, 2);
    path.lineTo(3, 4);
    path.cubicTo(5, 6, 7, 8, 9, 10);
    PK_COMPARE(path.elementCount(), 5);

    PK_VERIFY(path.elementAt(0).isMoveTo());
    PK_VERIFY(path.elementAt(1).isLineTo());
    PK_VERIFY(path.elementAt(2).isCurveTo());         // CurveToElement
    PK_VERIFY(!path.elementAt(3).isCurveTo());         // CurveToDataElement
    PK_VERIFY(!path.elementAt(4).isCurveTo());         // CurveToDataElement
}

void PkPainterPathCase::setElementPositionAt()
{
    PkPainterPath path;
    path.moveTo(10, 20);
    path.lineTo(30, 40);

    path.setElementPositionAt(0, 100, 200);
    PK_COMPARE(path.elementAt(0).x, 100.0);
    PK_COMPARE(path.elementAt(0).y, 200.0);

    // 修改后缓存应重新计算
    PkRectF bounds = path.boundingRect();
    PK_VERIFY(bounds.x() <= 100);
}

// ============================================================================
// 填充规则
// ============================================================================

void PkPainterPathCase::fillRule()
{
    PkPainterPath path;
    PK_COMPARE(path.fillRule(), Qt::OddEvenFill);

    path.setFillRule(Qt::WindingFill);
    PK_COMPARE(path.fillRule(), Qt::WindingFill);

    path.setFillRule(Qt::OddEvenFill);
    PK_COMPARE(path.fillRule(), Qt::OddEvenFill);
}

// ============================================================================
// 简单形状附加
// ============================================================================

void PkPainterPathCase::addRect()
{
    PkPainterPath path;
    path.addRect(PkRectF(0, 0, 100, 50));
    PK_VERIFY(!path.isEmpty());
    PK_VERIFY(path.isClosed());
    PK_COMPARE(path.elementCount(), 5); // moveTo + 3 lineTo + closeSubpath 的 lineTo
    PK_COMPARE(path.boundingRect(), PkRectF(0, 0, 100, 50));

    // qreal 重载
    PkPainterPath path2;
    path2.addRect(10, 20, 30, 40);
    PK_COMPARE(path2.boundingRect(), PkRectF(10, 20, 30, 40));
}

void PkPainterPathCase::addPolygon()
{
    PkPolygonF poly;
    poly << PkPointF(0, 0) << PkPointF(100, 0) << PkPointF(50, 100);

    PkPainterPath path;
    path.addPolygon(poly);
    PK_VERIFY(!path.isEmpty());
    PK_VERIFY(path.isClosed());

    // 空多边形不做任何事
    PkPolygonF emptyPoly;
    PkPainterPath path2;
    path2.addPolygon(emptyPoly);
    PK_VERIFY(path2.isEmpty());
}

void PkPainterPathCase::addPath()
{
    PkPainterPath path1;
    path1.addRect(PkRectF(0, 0, 50, 50));
    path1.setFillRule(Qt::WindingFill);

    PkPainterPath path2;
    path2.addRect(PkRectF(100, 100, 30, 30));

    path1.addPath(path2);
    PK_COMPARE(path1.elementCount(), 10); // 两个矩形各 5 个元素
    PK_COMPARE(path1.fillRule(), Qt::WindingFill); // 继承 path1 的 fillRule
}

// ============================================================================
// 变换
// ============================================================================

void PkPainterPathCase::translate()
{
    PkPainterPath path;
    path.addRect(PkRectF(0, 0, 10, 10));

    path.translate(5, 10);
    PK_COMPARE(path.boundingRect(), PkRectF(5, 10, 10, 10));

    // 零位移不做任何事
    path.translate(0, 0);
    PK_COMPARE(path.boundingRect(), PkRectF(5, 10, 10, 10));

    // PkPointF 重载
    PkPainterPath path2;
    path2.addRect(0, 0, 1, 1);
    path2.translate(PkPointF(100, 200));
    PK_COMPARE(path2.boundingRect(), PkRectF(100, 200, 1, 1));
}

void PkPainterPathCase::translated()
{
    PkPainterPath path;
    path.addRect(PkRectF(0, 0, 10, 10));

    PkPainterPath translated = path.translated(5, 10);
    // 原路径不变
    PK_COMPARE(path.boundingRect(), PkRectF(0, 0, 10, 10));
    // 新路径已平移
    PK_COMPARE(translated.boundingRect(), PkRectF(5, 10, 10, 10));
}

// ============================================================================
// 比较
// ============================================================================

void PkPainterPathCase::equality()
{
    PkPainterPath a;
    a.addRect(PkRectF(0, 0, 10, 10));

    PkPainterPath b;
    b.addRect(PkRectF(0, 0, 10, 10));
    PK_VERIFY(a == b);
    PK_VERIFY(!(a != b));

    // 不同内容
    PkPainterPath c;
    c.addRect(PkRectF(0, 0, 20, 20));
    PK_VERIFY(a != c);

    // 不同 fillRule
    PkPainterPath d;
    d.addRect(PkRectF(0, 0, 10, 10));
    d.setFillRule(Qt::WindingFill);
    PK_VERIFY(a != d);

    // 空路径相等
    PkPainterPath empty1;
    PkPainterPath empty2;
    PK_VERIFY(empty1 == empty2);
}

// ============================================================================
// T2 形状辅助
// ============================================================================

void PkPainterPathCase::addEllipse()
{
    PkPainterPath path;
    path.addEllipse(PkRectF(0, 0, 100, 100));
    // 一个椭圆由 4 段三次曲线组成：1 moveTo + 4 * (1 CurveToElement + 2 CurveToDataElement)
    // = 1 + 12 = 13 个元素
    PK_VERIFY(!path.isEmpty());
    PK_VERIFY(path.isClosed());

    // 椭圆包围盒应接近输入矩形
    PkRectF bounds = path.boundingRect();
    PK_VERIFY(bounds.x() >= -1 && bounds.x() <= 0);
    PK_VERIFY(bounds.y() >= -1 && bounds.y() <= 0);
    PK_VERIFY(bounds.x() + bounds.width() >= 100);
    PK_VERIFY(bounds.y() + bounds.height() >= 100);

    // qreal 重载
    PkPainterPath path2;
    path2.addEllipse(10, 20, 50, 30);
    PK_VERIFY(!path2.isEmpty());

    // PkPointF 重载
    PkPainterPath path3;
    path3.addEllipse(PkPointF(50, 50), 30, 20);
    PK_VERIFY(!path3.isEmpty());

    // 空矩形不做任何事
    PkPainterPath path4;
    path4.addEllipse(PkRectF(0, 0, 0, 0));
    PK_VERIFY(path4.isEmpty());
}

void PkPainterPathCase::arcTo()
{
    PkPainterPath path;
    path.arcTo(PkRectF(0, 0, 100, 100), 0, 90);
    // 90 度弧：arcTo 先 lineTo 到弧起点，再加一段三次曲线
    // 路径为空时先隐式 moveTo(0,0) 再 lineTo(curve_start)
    PK_VERIFY(!path.isEmpty());

    // 空矩形不做任何事
    PkPainterPath path2;
    path2.arcTo(PkRectF(0, 0, 0, 0), 0, 90);
    PK_VERIFY(path2.isEmpty());

    // qreal 重载
    PkPainterPath path3;
    path3.arcTo(0, 0, 100, 100, 0, 360);
    PK_VERIFY(!path3.isEmpty());
}

void PkPainterPathCase::addRoundedRect()
{
    PkPainterPath path;
    path.addRoundedRect(PkRectF(0, 0, 100, 100), 20, 20);
    PK_VERIFY(!path.isEmpty());
    PK_VERIFY(path.isClosed());

    // 包围盒接近输入矩形
    PkRectF bounds = path.boundingRect();
    PK_VERIFY(bounds.x() <= 0);
    PK_VERIFY(bounds.y() <= 0);
    PK_VERIFY(bounds.x() + bounds.width() >= 100);
    PK_VERIFY(bounds.y() + bounds.height() >= 100);

    // 零半径退化为普通矩形
    PkPainterPath path2;
    path2.addRoundedRect(PkRectF(0, 0, 100, 100), 0, 0);
    PK_VERIFY(!path2.isEmpty());
    PK_COMPARE(path2.boundingRect(), PkRectF(0, 0, 100, 100));

    // 空矩形不做任何事
    PkPainterPath path3;
    path3.addRoundedRect(PkRectF(0, 0, 0, 0), 10, 10);
    PK_VERIFY(path3.isEmpty());
}

// ============================================================================
// R-39: 面积布尔运算
// ============================================================================

namespace {

PkPainterPath r39Rectangle(qreal x, qreal y, qreal width, qreal height)
{
    PkPainterPath path;
    path.addRect(PkRectF(x, y, width, height));
    return path;
}

PkPainterPath r39Donut(Qt::FillRule rule)
{
    PkPainterPath path;
    path.setFillRule(rule);
    path.addRect(PkRectF(0, 0, 10, 10));
    path.addRect(PkRectF(3, 3, 4, 4));
    return path;
}

PkPainterPath r39BowTie(Qt::FillRule rule)
{
    PkPainterPath path;
    path.setFillRule(rule);
    path.moveTo(0, 0);
    path.lineTo(10, 10);
    path.lineTo(0, 10);
    path.lineTo(10, 0);
    path.closeSubpath();
    return path;
}

void verifyR39Membership(const PkPainterPath &path, const char *bits)
{
    const PkPointF probes[] = {
        PkPointF(1, 1), PkPointF(4, 4), PkPointF(5, 5),
        PkPointF(8, 5), PkPointF(12, 5), PkPointF(5, 9)
    };
    for (int i = 0; i < 6; ++i)
        PK_COMPARE(path.contains(probes[i]), bits[i] == '1');
}

} // namespace

void PkPainterPathCase::booleanEmptyIdentities()
{
    const PkPainterPath empty;
    PkPainterPath moveOnly;
    moveOnly.moveTo(17, -23);
    PkPainterPath shape = r39Rectangle(0, 0, 8, 10);
    shape.setFillRule(Qt::WindingFill);

    PK_VERIFY(empty.intersected(shape).isEmpty());
    PK_VERIFY(shape.intersected(empty).isEmpty());
    PK_COMPARE(empty.united(shape), shape);
    PK_COMPARE(shape.united(empty), shape);
    PK_VERIFY(empty.subtracted(shape).isEmpty());
    PK_COMPARE(shape.subtracted(empty), shape);
    PK_VERIFY(empty.simplified().isEmpty());

    PK_VERIFY(moveOnly.isEmpty());
    PK_COMPARE(moveOnly.elementCount(), 1);
    PK_COMPARE(moveOnly.united(shape), shape);
    PK_COMPARE(shape.united(moveOnly), shape);
    PK_VERIFY(moveOnly.intersected(shape).isEmpty());
    PK_VERIFY(shape.intersected(moveOnly).isEmpty());
    PK_VERIFY(moveOnly.subtracted(shape).isEmpty());
    PK_COMPARE(shape.subtracted(moveOnly), shape);
    PK_COMPARE(moveOnly.simplified(), moveOnly);

    PK_COMPARE(moveOnly | shape, shape);
    PK_COMPARE(shape | moveOnly, shape);
    PK_COMPARE(moveOnly + shape, shape);
    PK_COMPARE(shape + moveOnly, shape);
    PK_VERIFY((moveOnly & shape).isEmpty());
    PK_VERIFY((shape & moveOnly).isEmpty());
    PK_VERIFY((moveOnly - shape).isEmpty());
    PK_COMPARE(shape - moveOnly, shape);
}

void PkPainterPathCase::equalityUsesBoundingSizeEpsilon()
{
    const PkPainterPath empty;
    PkPainterPath originMove;
    originMove.moveTo(0, 0);
    PkPainterPath displacedMove;
    displacedMove.moveTo(17, -23);
    PK_VERIFY(empty == originMove);
    PK_VERIFY(empty != displacedMove);

    PkPainterPath smallA = r39Rectangle(10.0, 0.0, 1.0, 2.0);
    PkPainterPath smallB = r39Rectangle(10.1, 0.0, 1.0, 2.0);
    PK_VERIFY(smallA != smallB);

    PkPainterPath largeA = r39Rectangle(1.0e6, 0.0, 1.0, 2.0);
    PkPainterPath largeB = r39Rectangle(1.0e6 + 0.0000005, 0.0, 1.0, 2.0);
    PK_VERIFY(largeA != largeB);
}

void PkPainterPathCase::booleanRectanglesAndOperators()
{
    const PkPainterPath left = r39Rectangle(0, 0, 8, 10);
    const PkPainterPath right = r39Rectangle(4, 0, 8, 10);

    const PkPainterPath united = left.united(right);
    PK_COMPARE(united.fillRule(), Qt::OddEvenFill);
    PK_COMPARE(united.elementCount(), 5);
    PK_COMPARE(united.boundingRect(), PkRectF(0, 0, 12, 10));
    verifyR39Membership(united, "111101");

    const PkPainterPath intersected = left.intersected(right);
    PK_COMPARE(intersected.fillRule(), Qt::OddEvenFill);
    PK_COMPARE(intersected.elementCount(), 5);
    PK_COMPARE(intersected.boundingRect(), PkRectF(4, 0, 4, 10));
    verifyR39Membership(intersected, "011001");

    const PkPainterPath subtracted = left.subtracted(right);
    PK_COMPARE(subtracted.fillRule(), Qt::OddEvenFill);
    PK_COMPARE(subtracted.elementCount(), 5);
    PK_COMPARE(subtracted.boundingRect(), PkRectF(0, 0, 4, 10));
    verifyR39Membership(subtracted, "100000");

    PK_COMPARE(left & right, intersected);
    PK_COMPARE(left | right, united);
    PK_COMPARE(left + right, united);
    PK_COMPARE(left - right, subtracted);

    PkPainterPath assigned = left;
    assigned &= right;
    PK_COMPARE(assigned, intersected);
    assigned = left;
    assigned |= right;
    PK_COMPARE(assigned, united);
    assigned = left;
    assigned += right;
    PK_COMPARE(assigned, united);
    assigned = left;
    assigned -= right;
    PK_COMPARE(assigned, subtracted);
}

void PkPainterPathCase::booleanFillRulesAndCompounds()
{
    const PkPainterPath oddDonut = r39Donut(Qt::OddEvenFill);
    PK_COMPARE(oddDonut.elementCount(), 10);
    PK_COMPARE(oddDonut.boundingRect(), PkRectF(0, 0, 10, 10));
    verifyR39Membership(oddDonut, "100101");

    const PkPainterPath windingDonut = r39Donut(Qt::WindingFill);
    PK_COMPARE(windingDonut.elementCount(), 10);
    PK_COMPARE(windingDonut.boundingRect(), PkRectF(0, 0, 10, 10));
    verifyR39Membership(windingDonut, "111101");

    PkPainterPath compound;
    compound.addRect(PkRectF(0, 0, 3, 3));
    compound.addRect(PkRectF(7, 7, 3, 3));
    const PkPainterPath result = compound.intersected(r39Rectangle(1, 1, 8, 8));
    PK_COMPARE(result.fillRule(), Qt::OddEvenFill);
    PK_COMPARE(result.elementCount(), 9);
    PK_COMPARE(result.boundingRect(), PkRectF(1, 1, 8, 8));
    verifyR39Membership(result, "100000");
}

void PkPainterPathCase::simplifiedSelfIntersections()
{
    const PkPainterPath odd = r39BowTie(Qt::OddEvenFill).simplified();
    PK_COMPARE(odd.fillRule(), Qt::OddEvenFill);
    PK_COMPARE(odd.elementCount(), 8);
    PK_COMPARE(odd.boundingRect(), PkRectF(0, 0, 10, 10));
    verifyR39Membership(odd, "110001");

    const PkPainterPath winding = r39BowTie(Qt::WindingFill).simplified();
    PK_COMPARE(winding.fillRule(), Qt::OddEvenFill);
    PK_COMPARE(winding.elementCount(), 8);
    PK_COMPARE(winding.boundingRect(), PkRectF(0, 0, 10, 10));
    verifyR39Membership(winding, "110001");
}

void PkPainterPathCase::pathRelationQueries()
{
    const PkPainterPath outer = r39Rectangle(0, 0, 10, 10);
    const PkPainterPath inner = r39Rectangle(2, 2, 3, 3);
    const PkPainterPath overlap = r39Rectangle(8, 2, 5, 5);
    const PkPainterPath disjoint = r39Rectangle(20, 20, 2, 2);

    PK_VERIFY(outer.contains(inner));
    PK_VERIFY(!inner.contains(outer));
    PK_VERIFY(outer.intersects(inner));
    PK_VERIFY(outer.intersects(overlap));
    PK_VERIFY(!outer.intersects(disjoint));
    PK_VERIFY(!outer.contains(overlap));
}

int run_painterpath_tests()
{
    PkPainterPathCase tc;
    const char *argv[] = {"test_pkgeometry"};
    return PkTest::qExec(&tc, 1, const_cast<char **>(argv));
}
