#ifndef KIS_BASIC_TOOLS_GEOMETRY_UTILS_H
#define KIS_BASIC_TOOLS_GEOMETRY_UTILS_H

#include <PkPainterPath.h>
#include <PkTransform.h>

#include <cmath>

namespace KisBasicToolsGeometry
{

inline void rotateRadians(PkTransform &transform, qreal angle)
{
    const qreal sine = std::sin(angle);
    const qreal cosine = std::cos(angle);
    const PkTransform rotation(cosine, sine, -sine, cosine, 0.0, 0.0);
    transform = rotation * transform;
}

inline void rotateDegrees(PkTransform &transform, qreal angle)
{
    if (angle == 0.0) {
        return;
    }

    qreal sine = 0.0;
    qreal cosine = 0.0;
    if (angle == 90.0 || angle == -270.0) {
        sine = 1.0;
    } else if (angle == 270.0 || angle == -90.0) {
        sine = -1.0;
    } else if (angle == 180.0) {
        cosine = -1.0;
    } else {
        constexpr qreal degreesToRadians = 0.017453292519943295769;
        sine = std::sin(degreesToRadians * angle);
        cosine = std::cos(degreesToRadians * angle);
    }

    const PkTransform rotation(cosine, sine, -sine, cosine, 0.0, 0.0);
    transform = rotation * transform;
}

inline void addAbsoluteRoundedRect(PkPainterPath &path,
                                   const PkRectF &rect,
                                   qreal xRadius,
                                   qreal yRadius)
{
    const PkRectF normalizedRect = rect.normalized();
    if (normalizedRect.isNull()) {
        return;
    }

    const qreal halfWidth = normalizedRect.width() / 2.0;
    const qreal halfHeight = normalizedRect.height() / 2.0;
    xRadius = halfWidth ? 100.0 * qMin(xRadius, halfWidth) / halfWidth : 0.0;
    yRadius = halfHeight ? 100.0 * qMin(yRadius, halfHeight) / halfHeight : 0.0;
    if (xRadius <= 0.0 || yRadius <= 0.0) {
        path.addRect(normalizedRect);
        return;
    }

    const qreal x = normalizedRect.x();
    const qreal y = normalizedRect.y();
    const qreal width = normalizedRect.width();
    const qreal height = normalizedRect.height();
    const qreal doubledXRadius = width * xRadius / 100.0;
    const qreal doubledYRadius = height * yRadius / 100.0;

    path.moveTo(x, y + doubledYRadius / 2.0);
    path.arcTo(x, y, doubledXRadius, doubledYRadius, 180.0, -90.0);
    path.arcTo(x + width - doubledXRadius, y, doubledXRadius, doubledYRadius, 90.0, -90.0);
    path.arcTo(x + width - doubledXRadius, y + height - doubledYRadius,
               doubledXRadius, doubledYRadius, 0.0, -90.0);
    path.arcTo(x, y + height - doubledYRadius,
               doubledXRadius, doubledYRadius, 270.0, -90.0);
    path.closeSubpath();
}

}

#endif // KIS_BASIC_TOOLS_GEOMETRY_UTILS_H
