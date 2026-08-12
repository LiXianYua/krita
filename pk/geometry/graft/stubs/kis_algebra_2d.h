#pragma once
// ============================================================================
// 试接垫片 —— **不是 R-03 的交付物**。
//
// 真品 libs/global/kis_algebra_2d.h 有 1 254 行，头部 include 里带着
// `<QPolygonF>` `<QPainterPath>` `<boost/optional.hpp>` `<QTransform>`，
// 是一整个二维代数工具库（KisAlgebra2D 命名空间下几十个模板 + 若干导出函数
// + KisAlgebra2D::LineIntersection / DecomposedMatrix 等类）。它自己是一个
// **独立的迁移单元**，既不属于 R-03（几何 POD 类型）也不属于 R-02（容器）——
// 归属见 docs/boundary-analysis/，在它被单独立项之前，试接只能垫。
//
// 两个试接目标真正用到的只有三个模板（口径：被测源与测试源全文 grep
// `KisAlgebra2D::`）：
//   · blowRect     —— libs/global/KisRectsGrid.cpp:70（目标①）
//   · crossProduct —— libs/image/kis_four_point_interpolator_backward.h:61（目标②）
//   · normSquared  —— libs/image/kis_four_point_interpolator_backward.h:192（目标②）
//
// 三个函数体与 PointTypeTraits **逐字抄自真品**（行号标在各项上方），不是重写：
// crossProduct 与 normSquared 直接进入目标② 的算法取值，重写一遍等于把被测算法
// 的一部分换成我们自己的实现，那样试接就不再证明任何事。
// ============================================================================
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <kis_global.h>   // pow2 —— 真品也是从这里拿的

namespace KisAlgebra2D {

// kis_algebra_2d.h:28-47
template <class T>
struct PointTypeTraits
{
};

template <>
struct PointTypeTraits<QPoint>
{
    typedef int value_type;
    typedef qreal calculation_type;
    typedef QRect rect_type;
};

template <>
struct PointTypeTraits<QPointF>
{
    typedef qreal value_type;
    typedef qreal calculation_type;
    typedef QRectF rect_type;
};

// kis_algebra_2d.h:56-60
template <class T>
typename PointTypeTraits<T>::value_type crossProduct(const T &a, const T &b)
{
    return a.x() * b.y() - a.y() * b.x();
}

// kis_algebra_2d.h:68-72
template <class T>
qreal normSquared(const T &a)
{
    return pow2(a.x()) + pow2(a.y());
}

// kis_algebra_2d.h:353-363
template <class Rect>
Rect blowRect(const Rect &rect, qreal coeff)
{
    typedef decltype(rect.x()) CoordType;

    CoordType w = rect.width() * coeff;
    CoordType h = rect.height() * coeff;

    return rect.adjusted(-w, -h, w, h);
}

}
