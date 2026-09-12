/*
 * SPDX-FileCopyrightText: 2025 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "SvgChangeTextPaddingMarginStrategy.h"

#include <KoPathShape.h>
#include <KoPathSegment.h>

#include <KoSvgTextProperties.h>
#include <KisHandlePainterHelper.h>

#include "SvgTextMergePropertiesRangeCommand.h"

#include <kis_global.h>

// qt 桶：<QtGlobal> 取真 Qt 的 qglobal.h，提供 Q_FOREACH
#include <QtGlobal>

SvgChangeTextPaddingMarginStrategy::SvgChangeTextPaddingMarginStrategy(KoToolBase *tool, KoSvgTextShape *shape, const PkPointF &clicked)
    : KoInteractionStrategy(tool)
    , m_shape(shape)
    , m_lastMousePos(clicked)
{
    KoSvgTextProperties props = m_shape->textProperties();
    props.inheritFrom(KoSvgTextProperties::defaultProperties(), true);

    const qreal shapePadding = props.propertyOrDefault(KoSvgTextProperties::ShapePaddingId).value<KoSvgText::CssLengthPercentage>().value;
    const qreal shapeMargin = props.propertyOrDefault(KoSvgTextProperties::ShapeMarginId).value<KoSvgText::CssLengthPercentage>().value;

    const PkPointF padding(shapePadding+2, shapePadding+2);
    const PkPointF margin(shapeMargin+2, shapeMargin+2);

    qreal minDistance = std::numeric_limits<qreal>::max();
    KoPathShape *candidate = nullptr;
    bool isPadding = false;
    Q_FOREACH(KoShape *shape, m_shape->shapesSubtract()) {
        KoPathShape *path = dynamic_cast<KoPathShape*>(shape);
        if (!path) continue;
        const PkPointF mp = shape->documentToShape(clicked);
        const PkRectF marginRect(mp - margin, mp + margin);

        Q_FOREACH(KoPathSegment segment, path->segmentsAt(marginRect)) {
            const qreal nearestT = segment.nearestPoint(mp);
            const PkPointF nearestP = segment.pointAt(nearestT);
            const qreal distance = kisDistance(mp, nearestP) - shapeMargin;
            if (distance < minDistance) {
                candidate = path;
                minDistance = distance;
            }
        }
    }

    Q_FOREACH(KoShape *shape, m_shape->shapesInside()) {
        KoPathShape *path = dynamic_cast<KoPathShape*>(shape);
        if (!path) continue;
        const PkPointF mp = shape->documentToShape(clicked);
        const PkRectF paddingRect(mp - padding, mp + padding);

        Q_FOREACH(KoPathSegment segment, path->segmentsAt(paddingRect)) {
            const qreal nearestT = segment.nearestPoint(mp);
            const PkPointF nearestP = segment.pointAt(nearestT);
            const qreal distance = kisDistance(mp, nearestP) - shapePadding;
            if (distance < minDistance) {
                candidate = path;
                minDistance = distance;
                isPadding = true;
            }
        }
    }

    m_referenceShape = candidate;
    m_isPadding = isPadding;
}

SvgChangeTextPaddingMarginStrategy::~SvgChangeTextPaddingMarginStrategy()
{

}

std::optional<PkPointF> SvgChangeTextPaddingMarginStrategy::hitTest(KoSvgTextShape *shape, const PkPointF &mousePos, const qreal grabSensitivityInPts)
{
    if (!shape) return std::nullopt;
    const PkList<PkPainterPath> textAreas = shape->textWrappingAreas();
    if (textAreas.isEmpty()) return std::nullopt;

    const PkPointF grab(grabSensitivityInPts, grabSensitivityInPts);
    const PkRectF grabRect(mousePos-grab, mousePos+grab);

    Q_FOREACH(const PkPainterPath area, shape->textWrappingAreas()) {
        KoPathShape *s = KoPathShape::createShapeFromPainterPath(area);
        if (!s) continue;
        s->setTransformation(shape->absoluteTransformation());
        KoPathSegment segment = s->segmentAtPoint(mousePos, grabRect);
        if (segment.isValid()) {
            qreal nearest = segment.nearestPoint(s->documentToShape(mousePos));
            return std::make_optional(segment.angleVectorAtParam(nearest));
        }
    }

    return std::nullopt;
}

PkLineF getLine(PkPointF mousePos, KoPathShape *referenceShape, bool isPadding) {
    if (!referenceShape) return PkLineF();
    const bool hit = referenceShape->hitTest(mousePos);
    if ((!hit && isPadding) || (hit && !isPadding)) return PkLineF();

    PkPointF pos = referenceShape->documentToShape(mousePos);
    PkLineF l(pos, pos);

    qreal minDistance = std::numeric_limits<qreal>::max();

    Q_FOREACH(KoPathSegment segment, referenceShape->segmentsAt(referenceShape->outlineRect().adjusted(-2, -2, 2, 2))) {
        const qreal nearestT = segment.nearestPoint(pos);
        const PkPointF nearestP = segment.pointAt(nearestT);
        const qreal distance = kisDistance(pos, nearestP);
        if (distance < minDistance) {
            l.setP1(nearestP);
            minDistance = distance;
        }
    }
    if (l.length() < 0) {
        l.setLength(0);
    }
    return l;
}

KoSvgTextProperties getProperties(bool isPadding, PkLineF line, KoSvgTextProperties previous = KoSvgTextProperties()) {
    KoSvgTextProperties::PropertyId propId = isPadding? KoSvgTextProperties::ShapePaddingId: KoSvgTextProperties::ShapeMarginId;
    KoSvgText::CssLengthPercentage length;
    length.value = line.length();
    previous.setProperty(propId, PkVariant::fromValue(length));
    return previous;
}

void SvgChangeTextPaddingMarginStrategy::paint(PkPainter &painter, const KoViewConverter &converter)
{
    if (!(m_referenceShape && m_shape)) return;
    painter.save();
    KisHandlePainterHelper handlePainter =
            KoShape::createHandlePainterHelperView(&painter, m_shape, converter, handleRadius(), decorationThickness());
    handlePainter.setHandleStyle(KisHandleStyle::selectedPrimaryHandles());

    const PkLineF line = getLine(m_lastMousePos, m_referenceShape, m_isPadding);
    const PkTransform lineTf = m_referenceShape->absoluteTransformation() * m_shape->absoluteTransformation().inverted();
    handlePainter.drawConnectionLine(lineTf.map(line));

    KoSvgTextProperties props = getProperties(m_isPadding, line, m_shape->textProperties());
    PkList<PkPainterPath> areas = m_shape->generateTextAreas(m_shape->shapesInside(), m_shape->shapesSubtract(), props);
    Q_FOREACH(PkPainterPath area, areas) {
        handlePainter.drawPath(area);
    }

    painter.restore();
}

void SvgChangeTextPaddingMarginStrategy::handleMouseMove(const PkPointF &mouseLocation, Pk::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers);
    m_lastMousePos = mouseLocation;
}

KUndo2Command *SvgChangeTextPaddingMarginStrategy::createCommand()
{
    if (!(m_referenceShape && m_shape)) return nullptr;
    const PkLineF l = getLine(m_lastMousePos, m_referenceShape, m_isPadding);
    KoSvgTextProperties props = getProperties(m_isPadding, l);
    return new SvgTextMergePropertiesRangeCommand(m_shape, props, -1, -1);
}

void SvgChangeTextPaddingMarginStrategy::finishInteraction(Pk::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers);
}
