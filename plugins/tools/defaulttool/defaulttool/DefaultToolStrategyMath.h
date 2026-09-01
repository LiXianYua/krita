#pragma once

#include <PkPainterPath.h>
#include <PkPoint.h>
#include <PkPolygon.h>

#include <cmath>

namespace DefaultToolStrategyMath {

inline PkPointF moveDelta(const PkPointF &start, const PkPointF &current)
{
    return current - start;
}

inline double resizeScale(double initialExtent, double draggedExtent)
{
    return initialExtent == 0.0 ? 1.0 : draggedExtent / initialExtent;
}

inline double snappedRotationDegrees(double angle, bool constrained)
{
    if (!constrained) return angle;
    double remainder = std::abs(angle);
    while (remainder > 45.0) {
        remainder -= 45.0;
    }
    if (remainder > 22.5) {
        remainder -= 45.0;
    }
    return angle + (angle > 0.0 ? -1.0 : 1.0) * remainder;
}

inline double shearFactor(double delta, double extent)
{
    return extent == 0.0 ? 0.0 : delta / extent;
}

inline PkPointF gradientHandlePosition(const PkPointF &position, const PkPointF &offset)
{
    return position + offset;
}

inline PkPointF meshHandlePosition(const PkPointF &position, const PkPointF &offset)
{
    return position + offset;
}

inline bool polygonLeavesBoundingRect(const PkPolygonF &polygon)
{
    PkPainterPath bounds;
    bounds.addRect(polygon.boundingRect());
    PkPainterPath outline;
    outline.addPolygon(polygon);
    return !bounds.subtracted(outline).isEmpty();
}

}
