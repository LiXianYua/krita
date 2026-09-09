/* This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2006-2007 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2006-2008 Jan Hambrecht <jaham@gmx.net>
 * SPDX-FileCopyrightText: 2007, 2009 Thorsten Zachmann <zachmann@kde.org>
 * SPDX-FileCopyrightText: 2012 Inge Wallin <inge@lysator.liu.se>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

// Own
#include "KoShapeStroke.h"
#include <PkFlakeBridge.h>

// Posix
#include <math.h>

// Qt
#include <PkPainterPath.h>
#include <PkPainter.h>

// Calligra

// Flake
#include "KoShape.h"
#include "KoShapeSavingContext.h"
#include "KoPathShape.h"
#include "KoMarker.h"
#include "KoInsets.h"
#include <KoPathSegment.h>
#include <KoPathPoint.h>
#include <cmath>
#include <algorithm>
#include <PkContainerAlgo.h>
#include "KisQPainterStateSaver.h"

#include "kis_global.h"

class KoShapeStroke::Private
{
public:
    Private(KoShapeStroke *_q) : q(_q) {}
    KoShapeStroke *q;

    void paintBorder(const KoShape *shape, PkPainter &painter, const PkPen &pen) const;
    void paintMarkers(const KoShape *shape, PkPainter &painter, const PkPen &pen) const;
    PkColor color;
    PkPen pen;
    PkBrush brush;
};

namespace {
std::pair<qreal, qreal> anglesForSegment(KoPathSegment segment) {
    const qreal eps = 1e-6;

    if (segment.degree() < 3) {
        segment = segment.toCubic();
    }

    PkList<PkPointF> points = segment.controlPoints();
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(points.size() == 4, std::make_pair(0.0, 0.0));
    PkPointF vec1 = points[1] - points[0];
    PkPointF vec2 = points[3] - points[2];

    if (vec1.manhattanLength() < eps) {
        points[1] = segment.pointAt(eps);
        vec1 = points[1] - points[0];
    }

    if (vec2.manhattanLength() < eps) {
        points[2] = segment.pointAt(1.0 - eps);
        vec2 = points[3] - points[2];
    }

    const qreal angle1 = std::atan2(vec1.y(), vec1.x());
    const qreal angle2 = std::atan2(vec2.y(), vec2.x());
    return std::make_pair(angle1, angle2);
}
}

void KoShapeStroke::Private::paintBorder(const KoShape *shape, PkPainter &painter, const PkPen &pen) const
{
    if (!pen.isCosmetic() && pen.style() != Pk::NoPen) {
        const KoPathShape *pathShape = dynamic_cast<const KoPathShape *>(shape);
        if (pathShape) {
            PkPainterPath path = pathShape->pathStroke(pen);

            painter.fillPath(path, pen.brush());

            return;
        }

        painter.strokePath(shape->outline(), pen);
    }
}
void KoShapeStroke::Private::paintMarkers(const KoShape *shape, PkPainter &painter, const PkPen &pen) const
{
    if (!pen.isCosmetic() && pen.style() != Pk::NoPen) {
        const KoPathShape *pathShape = dynamic_cast<const KoPathShape *>(shape);
        if (pathShape) {

            if (!pathShape->hasMarkers()) return;

            const bool autoFillMarkers = pathShape->autoFillMarkers();
            KoMarker *startMarker = pathShape->marker(KoFlake::StartMarker);
            KoMarker *midMarker = pathShape->marker(KoFlake::MidMarker);
            KoMarker *endMarker = pathShape->marker(KoFlake::EndMarker);

            for (int i = 0; i < pathShape->subpathCount(); i++) {
                const int numSubPoints = pathShape->subpathPointCount(i);
                if (numSubPoints < 2) continue;

                const bool isClosedSubpath = pathShape->isClosedSubpath(i);

                qreal firstAngle = 0.0;
                {
                    KoPathSegment segment = pathShape->segmentByIndex(KoPathPointIndex(i, 0));
                    firstAngle= anglesForSegment(segment).first;
                }

                const int numSegments = isClosedSubpath ? numSubPoints : numSubPoints - 1;

                qreal lastAngle = 0.0;
                {
                    KoPathSegment segment = pathShape->segmentByIndex(KoPathPointIndex(i, numSegments - 1));
                    lastAngle = anglesForSegment(segment).second;
                }

                qreal previousAngle = 0.0;

                for (int j = 0; j < numSegments; j++) {
                    KoPathSegment segment = pathShape->segmentByIndex(KoPathPointIndex(i, j));
                    std::pair<qreal, qreal> angles = anglesForSegment(segment);

                    const qreal angle1 = angles.first;
                    const qreal angle2 = angles.second;

                    if (j == 0 && startMarker) {
                        const qreal angle = isClosedSubpath ? bisectorAngle(firstAngle, lastAngle) : firstAngle;
                        if (autoFillMarkers) {
                            startMarker->applyShapeStroke(shape, q, segment.first()->point(), pen.widthF(), angle);
                        }
                        startMarker->paintAtPosition(&painter, segment.first()->point(), pen.widthF(), angle);
                    }

                    if (j > 0 && midMarker) {
                        const qreal angle = bisectorAngle(previousAngle, angle1);
                        if (autoFillMarkers) {
                            midMarker->applyShapeStroke(shape, q, segment.first()->point(), pen.widthF(), angle);
                        }
                        midMarker->paintAtPosition(&painter, segment.first()->point(), pen.widthF(), angle);
                    }

                    if (j == numSegments - 1 && endMarker) {
                        const qreal angle = isClosedSubpath ? bisectorAngle(firstAngle, lastAngle) : lastAngle;
                        if (autoFillMarkers) {
                            endMarker->applyShapeStroke(shape, q, segment.second()->point(), pen.widthF(), angle);
                        }
                        endMarker->paintAtPosition(&painter, segment.second()->point(), pen.widthF(), angle);
                    }

                    previousAngle = angle2;
                }
            }
        }
    }
}

KoShapeStroke::KoShapeStroke()
        : d(new Private(this))
{
    d->color = PkColor(Pk::black);
    // we are not rendering stroke with zero width anymore
    // so lets use a default width of 1.0
    d->pen.setWidthF(1.0);
}

KoShapeStroke::KoShapeStroke(const KoShapeStroke &other)
        : KoShapeStrokeModel(), d(new Private(this))
{
    d->color = other.d->color;
    d->pen = other.d->pen;
    d->brush = other.d->brush;
}

KoShapeStroke::KoShapeStroke(qreal lineWidth, const PkColor &color)
        : d(new Private(this))
{
    d->pen.setWidthF(std::max(qreal(0.0), lineWidth));
    d->pen.setJoinStyle(Pk::MiterJoin);
    d->color = color;
}

KoShapeStroke::~KoShapeStroke()
{
    delete d;
}

KoShapeStroke &KoShapeStroke::operator = (const KoShapeStroke &rhs)
{
    if (this == &rhs)
        return *this;

    d->pen = rhs.d->pen;
    d->color = rhs.d->color;
    d->brush = rhs.d->brush;

    return *this;
}

void KoShapeStroke::strokeInsets(const KoShape *shape, KoInsets &insets) const
{
    Q_UNUSED(shape);

    // '0.5' --- since we draw a line half inside, and half outside the object.
    qreal extent = 0.5 * (d->pen.widthF() >= 0 ? d->pen.widthF() : 1.0);

    // if we have square cap, we need a little more space
    // -> sqrt((0.5*penWidth)^2 + (0.5*penWidth)^2)
    if (capStyle() == Pk::SquareCap) {
        extent *= M_SQRT2;
    }

    if (joinStyle() == Pk::MiterJoin) {
        // miter limit in Qt is normalized by the line width (and not half-width)
        extent = std::max(extent, d->pen.widthF() * miterLimit());
    }

    insets.top = extent;
    insets.bottom = extent;
    insets.left = extent;
    insets.right = extent;
}

qreal KoShapeStroke::strokeMaxMarkersInset(const KoShape *shape) const
{
    qreal result = 0.0;

    const KoPathShape *pathShape = dynamic_cast<const KoPathShape *>(shape);
    if (pathShape && pathShape->hasMarkers()) {
        const qreal lineWidth = d->pen.widthF();

        PkVector<const KoMarker*> markers;
        markers << pathShape->marker(KoFlake::StartMarker);
        markers << pathShape->marker(KoFlake::MidMarker);
        markers << pathShape->marker(KoFlake::EndMarker);

        PK_FOREACH (const KoMarker *marker, markers) {
            if (marker) {
                result = std::max(result, marker->maxInset(lineWidth));
            }
        }
    }

    return result;
}

bool KoShapeStroke::hasTransparency() const
{
    return d->color.alpha() > 0;
}

PkPen KoShapeStroke::resultLinePen() const
{
    PkPen pen = d->pen;

    if (d->brush.gradient()) {
        pen.setBrush(d->brush);
    } else {
        pen.setColor(d->color.isValid() ? d->color : PkColor(Pk::transparent));
    }

    return pen;
}

void KoShapeStroke::paint(const KoShape *shape, PkPainter &painter) const
{
    KisQPainterStateSaver saver(&painter);

    d->paintBorder(shape, painter, resultLinePen());
}

void KoShapeStroke::paintMarkers(const KoShape *shape, PkPainter &painter) const
{
    KisQPainterStateSaver saver(&painter);

    d->paintMarkers(shape, painter, resultLinePen());
}

bool KoShapeStroke::compareFillTo(const KoShapeStrokeModel *other)
{
    if (!other) return false;

    const KoShapeStroke *stroke = dynamic_cast<const KoShapeStroke*>(other);
    if (!stroke) return false;

    return (d->brush.gradient() && d->brush == stroke->d->brush) ||
            (!d->brush.gradient() && d->color == stroke->d->color);
}

bool KoShapeStroke::compareStyleTo(const KoShapeStrokeModel *other)
{
    if (!other) return false;

    const KoShapeStroke *stroke = dynamic_cast<const KoShapeStroke*>(other);
    if (!stroke) return false;

    PkPen pen1 = d->pen;
    PkPen pen2 = stroke->d->pen;

    // just a random color top avoid comparison of that property
    pen1.setColor(PkColor(Pk::magenta));
    pen2.setColor(PkColor(Pk::magenta));

    return pen1 == pen2;
}

bool KoShapeStroke::isVisible() const
{
    return d->pen.widthF() > 0 &&
        (d->brush.gradient() || d->color.alpha() > 0);
}

void KoShapeStroke::setCapStyle(Pk::PenCapStyle style)
{
    d->pen.setCapStyle(style);
}

Pk::PenCapStyle KoShapeStroke::capStyle() const
{
    return d->pen.capStyle();
}

void KoShapeStroke::setJoinStyle(Pk::PenJoinStyle style)
{
    d->pen.setJoinStyle(style);
}

Pk::PenJoinStyle KoShapeStroke::joinStyle() const
{
    return d->pen.joinStyle();
}

void KoShapeStroke::setLineWidth(qreal lineWidth)
{
    d->pen.setWidthF(std::max(qreal(0.0), lineWidth));
}

qreal KoShapeStroke::lineWidth() const
{
    return d->pen.widthF();
}

void KoShapeStroke::setMiterLimit(qreal miterLimit)
{
    d->pen.setMiterLimit(miterLimit);
}

qreal KoShapeStroke::miterLimit() const
{
    return d->pen.miterLimit();
}

PkColor KoShapeStroke::color() const
{
    return d->color;
}

void KoShapeStroke::setColor(const PkColor &color)
{
    d->color = color;
}

void KoShapeStroke::setLineStyle(Pk::PenStyle style, const PkVector<qreal> &dashes)
{
    if (style < Pk::CustomDashLine) {
        d->pen.setStyle(style);
    } else {
        d->pen.setDashPattern(dashes);
    }
}

Pk::PenStyle KoShapeStroke::lineStyle() const
{
    return d->pen.style();
}

PkVector<qreal> KoShapeStroke::lineDashes() const
{
    {
        PkVector<qreal> out;
        for (qreal v : d->pen.dashPattern()) out.append(v);
        return out;
    }
}

void KoShapeStroke::setDashOffset(qreal dashOffset)
{
    d->pen.setDashOffset(dashOffset);
}

qreal KoShapeStroke::dashOffset() const
{
    return d->pen.dashOffset();
}

void KoShapeStroke::setLineBrush(const PkBrush &brush)
{
    d->brush = brush;
}

const PkBrush &KoShapeStroke::lineBrush() const
{
    return d->brush;
}
