/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "CalligraphyTool/KarbonCalligraphicShape.h"
#include "CalligraphyTool/KarbonSimplifyPath.h"
#include "KarbonToolsResources.h"

#include <KoPathPoint.h>
#include <KoPathShape.h>

#include <QByteArray>
#include <QCryptographicHash>
#include <QImage>

#include <cmath>

namespace
{
bool closeEnough(double lhs, double rhs)
{
    return std::abs(lhs - rhs) < 1e-12;
}

bool pointEquals(const PkPointF &point, double x, double y)
{
    return closeEnough(point.x(), x) && closeEnough(point.y(), y);
}

int calligraphicPointsPreserveLiteralGeometry()
{
    KarbonCalligraphicPoint point(PkPointF(10.0, 20.0), 0.0, 4.0);
    if (!pointEquals(point.point(), 10.0, 20.0)) return 10;
    if (!closeEnough(point.angle(), 0.0) || !closeEnough(point.width(), 4.0)) return 11;

    KarbonCalligraphicShape shape;
    shape.appendPointToPath(point);
    if (shape.pointCount() != 2) return 12;
    if (!pointEquals(shape.pointByIndex(KoPathPointIndex(0, 0))->point(), 8.0, 20.0)) return 13;
    if (!pointEquals(shape.pointByIndex(KoPathPointIndex(0, 1))->point(), 12.0, 20.0)) return 14;

    shape.appendPointToPath(KarbonCalligraphicPoint(PkPointF(20.0, 30.0), 0.0, 4.0));
    if (shape.pointCount() != 4) return 15;
    const PkPointF expected[] = {
        PkPointF(8.0, 20.0), PkPointF(18.0, 30.0),
        PkPointF(22.0, 30.0), PkPointF(12.0, 20.0)
    };
    for (int i = 0; i < 4; ++i) {
        if (shape.pointByIndex(KoPathPointIndex(0, i))->point() != expected[i]) return 16 + i;
    }
    return 0;
}

int simplifyRemovesDuplicateWithoutChangingEndpoints()
{
    KoPathShape path;
    path.moveTo(PkPointF(0.0, 0.0));
    path.lineTo(PkPointF(0.0, 0.0));
    path.lineTo(PkPointF(10.0, 0.0));
    karbonSimplifyPath(&path, 0.3);

    if (path.pointCount() != 2) return 30;
    if (!pointEquals(path.pointByIndex(KoPathPointIndex(0, 0))->point(), 0.0, 0.0)) return 31;
    if (!pointEquals(path.pointByIndex(KoPathPointIndex(0, 1))->point(), 10.0, 0.0)) return 32;
    return 0;
}

int nativeResourcePreservesCalligraphyIcon()
{
    const KarbonToolsResource icon = karbonCalligraphyIconPng();
    if (!icon.data || icon.size == 0) return 40;

    const QByteArray bytes(reinterpret_cast<const char *>(icon.data), int(icon.size));
    if (QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex() !=
        QByteArray("f3125eea25ad77c9bdf1a783ae7038f006d3c493d86a145e067315f43f7eea12")) return 41;

    const QImage image = QImage::fromData(bytes, "PNG");
    if (image.isNull() || image.width() != 22 || image.height() != 22) return 42;
    if (icon.size < 29 || bytes.mid(12, 4) != QByteArray("IHDR", 4)) return 43;
    if (icon.data[24] != 8 || icon.data[25] != 6 || icon.data[28] != 0) return 44;
    return 0;
}
} // namespace

int main()
{
    const int pointResult = calligraphicPointsPreserveLiteralGeometry();
    if (pointResult) return pointResult;
    const int simplifyResult = simplifyRemovesDuplicateWithoutChangingEndpoints();
    if (simplifyResult) return simplifyResult;
    return nativeResourcePreservesCalligraphyIcon();
}
