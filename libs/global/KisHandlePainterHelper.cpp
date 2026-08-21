#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QColor>
/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisHandlePainterHelper.h"


#include <PkPainterPath.h>
#include "kis_algebra_2d.h"
#include "kis_painting_tweaks.h"

using KisPaintingTweaks::PenBrushSaver;

KisHandlePainterHelper::KisHandlePainterHelper(PkPainter *_painter, qreal handleRadius, int decorationThickness)
    : m_painter(_painter),
      m_originalPainterTransform(m_painter->transform()),
      m_painterTransform(m_painter->transform()),
      m_handleRadius(handleRadius),
      m_decorationThickness(decorationThickness),
      m_decomposedMatrix(m_painterTransform)
{
    init();
}

KisHandlePainterHelper::KisHandlePainterHelper(PkPainter *_painter, const PkTransform &originalPainterTransform, qreal handleRadius, int decorationThickness)
    : m_painter(_painter),
      m_originalPainterTransform(originalPainterTransform),
      m_painterTransform(m_painter->transform()),
      m_handleRadius(handleRadius),
      m_decorationThickness(decorationThickness),
      m_decomposedMatrix(m_painterTransform)
{
    init();
}

KisHandlePainterHelper::KisHandlePainterHelper(KisHandlePainterHelper &&rhs)
    : m_painter(rhs.m_painter),
      m_originalPainterTransform(rhs.m_originalPainterTransform),
      m_painterTransform(rhs.m_painterTransform),
      m_handleRadius(rhs.m_handleRadius),
      m_decorationThickness(rhs.m_decorationThickness),
      m_decomposedMatrix(rhs.m_decomposedMatrix),
      m_handleTransform(rhs.m_handleTransform),
      m_handlePolygon(rhs.m_handlePolygon),
      m_handleStyle(rhs.m_handleStyle)
{
    // disable the source helper
    rhs.m_painter = 0;
}

void KisHandlePainterHelper::init()
{
    m_handleStyle = KisHandleStyle::inheritStyle();

    m_painter->setTransform(PkTransform());
    m_handleTransform = m_decomposedMatrix.shearTransform() * m_decomposedMatrix.rotateTransform();

    if (m_handleRadius > 0.0) {
        const PkRectF handleRect(-m_handleRadius, -m_handleRadius, 2 * m_handleRadius, 2 * m_handleRadius);
        m_handlePolygon = m_handleTransform.map(PkPolygonF(handleRect));
    }
}

KisHandlePainterHelper::~KisHandlePainterHelper() {
    if (m_painter) {
        m_painter->setTransform(m_originalPainterTransform);
    }
}

void KisHandlePainterHelper::setHandleStyle(const KisHandleStyle &style)
{
    m_handleStyle = style;
}

void KisHandlePainterHelper::drawHandleRect(const PkPointF &center, qreal radius, PkPoint offset = PkPoint(0,0))
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_painter);

    PkRectF handleRect(-radius, -radius, 2 * radius, 2 * radius);
    PkPolygonF handlePolygon = m_handleTransform.map(PkPolygonF(handleRect));
    handlePolygon.translate(m_painterTransform.map(center));

    handlePolygon.translate(offset);

    const PkPen originalPen = m_painter->pen();

    // temporarily set the pen width to 2 to avoid pixel shifting dropping pixels the border
    PkPen customPen = m_painter->pen();
    customPen.setCosmetic(true);
    customPen.setWidth(4);
    m_painter->setPen(customPen);

    for (KisHandleStyle::IterationStyle it : m_handleStyle.handleIterations) {
        it.stylePair.first.setWidthF(it.stylePair.first.widthF() * m_decorationThickness);
        PenBrushSaver saver(it.isValid ? m_painter : 0, it.stylePair, PenBrushSaver::allow_noop);
        m_painter->drawPolygon(handlePolygon);
    }

    m_painter->setPen(originalPen);
}

void KisHandlePainterHelper::drawHandleCircle(const PkPointF &center, qreal radius) {
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_painter);

    PkRectF handleRect(-radius, -radius, 2 * radius, 2 * radius);
    handleRect.translate(m_painterTransform.map(center));

    for (KisHandleStyle::IterationStyle it : m_handleStyle.handleIterations) {
        it.stylePair.first.setWidthF(it.stylePair.first.widthF() * m_decorationThickness);
        PenBrushSaver saver(it.isValid ? m_painter : 0, it.stylePair, PenBrushSaver::allow_noop);
        m_painter->drawEllipse(handleRect);
    }
}

void KisHandlePainterHelper::drawHandleCircle(const PkPointF &center)
{
    drawHandleCircle(center, m_handleRadius);
}

void KisHandlePainterHelper::drawHandleSmallCircle(const PkPointF &center)
{
    drawHandleCircle(center, 0.7 * m_handleRadius);
}

void KisHandlePainterHelper::drawHandleLine(const PkLineF &line, qreal width, PkVector<qreal> dashPattern, qreal dashOffset)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_painter);

    PkPainterPath p;
    p.moveTo(m_painterTransform.map(line.p1()));
    p.lineTo(m_painterTransform.map(line.p2()));
    PkPainterPathStroker s;
    s.setWidth(width);
    if (!dashPattern.isEmpty()) {
        s.setDashPattern(dashPattern);
        s.setDashOffset(dashOffset);
    }
    s.setCapStyle(Qt::RoundCap);
    s.setJoinStyle(Qt::RoundJoin);
    p = s.createStroke(p);

    for (KisHandleStyle::IterationStyle it : m_handleStyle.handleIterations) {
        it.stylePair.first.setWidthF(it.stylePair.first.widthF() * m_decorationThickness);
        PenBrushSaver saver(it.isValid ? m_painter : 0, it.stylePair, PenBrushSaver::allow_noop);
        m_painter->strokePath(p, m_painter->pen());
        m_painter->fillPath(p, m_painter->brush());
    }
}

void KisHandlePainterHelper::drawHandleRect(const PkPointF &center) {
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_painter);
    PkPolygonF paintingPolygon = m_handlePolygon.translated(m_painterTransform.map(center));

    for (KisHandleStyle::IterationStyle it : m_handleStyle.handleIterations) {
        it.stylePair.first.setWidthF(it.stylePair.first.widthF() * m_decorationThickness);
        PenBrushSaver saver(it.isValid ? m_painter : 0, it.stylePair, PenBrushSaver::allow_noop);
        m_painter->drawPolygon(paintingPolygon);
    }
}

void KisHandlePainterHelper::drawGradientHandle(const PkPointF &center, qreal radius) {
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_painter);

    PkPolygonF handlePolygon;

    handlePolygon << PkPointF(-radius, 0);
    handlePolygon << PkPointF(0, radius);
    handlePolygon << PkPointF(radius, 0);
    handlePolygon << PkPointF(0, -radius);

    handlePolygon = m_handleTransform.map(handlePolygon);
    handlePolygon.translate(m_painterTransform.map(center));

    for (KisHandleStyle::IterationStyle it : m_handleStyle.handleIterations) {
        it.stylePair.first.setWidthF(it.stylePair.first.widthF() * m_decorationThickness);
        PenBrushSaver saver(it.isValid ? m_painter : 0, it.stylePair, PenBrushSaver::allow_noop);
        m_painter->drawPolygon(handlePolygon);
    }
}

void KisHandlePainterHelper::drawGradientHandle(const PkPointF &center)
{
    drawGradientHandle(center, 1.41 * m_handleRadius);
}

void KisHandlePainterHelper::drawGradientCrossHandle(const PkPointF &center, qreal radius) {
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_painter);

    { // Draw a cross
        PkPainterPath p;
        p.moveTo(-radius, -radius);
        p.lineTo(radius, radius);
        p.moveTo(radius, -radius);
        p.lineTo(-radius, radius);

        p = m_handleTransform.map(p);
        p.translate(m_painterTransform.map(center));

        for (KisHandleStyle::IterationStyle it : m_handleStyle.handleIterations) {
            it.stylePair.first.setWidthF(it.stylePair.first.widthF() * m_decorationThickness);
            PenBrushSaver saver(it.isValid ? m_painter : 0, it.stylePair, PenBrushSaver::allow_noop);
            m_painter->drawPath(p);
        }
    }

    { // Draw a square
        const qreal halfRadius = 0.5 * radius;

        PkPolygonF handlePolygon;
        handlePolygon << PkPointF(-halfRadius, 0);
        handlePolygon << PkPointF(0, halfRadius);
        handlePolygon << PkPointF(halfRadius, 0);
        handlePolygon << PkPointF(0, -halfRadius);

        handlePolygon = m_handleTransform.map(handlePolygon);
        handlePolygon.translate(m_painterTransform.map(center));

        for (KisHandleStyle::IterationStyle it : m_handleStyle.handleIterations) {
            it.stylePair.first.setWidthF(it.stylePair.first.widthF() * m_decorationThickness);
            PenBrushSaver saver(it.isValid ? m_painter : 0, it.stylePair, PenBrushSaver::allow_noop);
            m_painter->drawPolygon(handlePolygon);
        }
    }
}

void KisHandlePainterHelper::drawArrow(const PkPointF &pos, const PkPointF &from, qreal radius)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_painter);

    PkPainterPath p;

    PkLineF line(pos, from);
    line.setLength(radius);

    PkPointF norm = KisAlgebra2D::leftUnitNormal(pos - from);
    norm *= 0.34 * radius;

    p.moveTo(line.p2() + norm);
    p.lineTo(line.p1());
    p.lineTo(line.p2() - norm);

    p.translate(-pos);

    p = m_handleTransform.map(p).translated(m_painterTransform.map(pos));

    for (KisHandleStyle::IterationStyle it : m_handleStyle.handleIterations) {
        it.stylePair.first.setWidthF(it.stylePair.first.widthF() * m_decorationThickness);
        PenBrushSaver saver(it.isValid ? m_painter : 0, it.stylePair, PenBrushSaver::allow_noop);
        m_painter->drawPath(p);
    }
}

void KisHandlePainterHelper::drawGradientArrow(const PkPointF &start, const PkPointF &end, qreal radius)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_painter);

    PkPainterPath p;
    p.moveTo(start);
    p.lineTo(end);
    p = m_painterTransform.map(p);

    for (KisHandleStyle::IterationStyle it : m_handleStyle.lineIterations) {
        it.stylePair.first.setWidthF(it.stylePair.first.widthF()*m_decorationThickness);
        PenBrushSaver saver(it.isValid ? m_painter : 0, it.stylePair, PenBrushSaver::allow_noop);
        m_painter->drawPath(p);
    }

    const qreal length = kisDistance(start, end);
    const PkPointF diff = end - start;

    if (length > 5 * radius) {
        drawArrow(start + 0.33 * diff, start, radius);
        drawArrow(start + 0.66 * diff, start, radius);
    } else if (length > 3 * radius) {
        drawArrow(start + 0.5 * diff, start, radius);
    }
}

void KisHandlePainterHelper::drawRubberLine(const PkPolygonF &poly) {
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_painter);

    PkPolygonF paintingPolygon = m_painterTransform.map(poly);

    for (KisHandleStyle::IterationStyle it : m_handleStyle.lineIterations) {
        it.stylePair.first.setWidthF(it.stylePair.first.widthF() * m_decorationThickness);
        PenBrushSaver saver(it.isValid ? m_painter : 0, it.stylePair, PenBrushSaver::allow_noop);
        m_painter->drawPolygon(paintingPolygon);
    }
}

void KisHandlePainterHelper::drawConnectionLine(const PkLineF &line)
{
    drawConnectionLine(line.p1(), line.p2());
}

void KisHandlePainterHelper::drawConnectionLine(const PkPointF &p1, const PkPointF &p2)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_painter);

    PkPointF realP1 = m_painterTransform.map(p1);
    PkPointF realP2 = m_painterTransform.map(p2);

    for (KisHandleStyle::IterationStyle it : m_handleStyle.lineIterations) {
        it.stylePair.first.setWidthF(it.stylePair.first.widthF() * m_decorationThickness);
        PenBrushSaver saver(it.isValid ? m_painter : 0, it.stylePair, PenBrushSaver::allow_noop);
        m_painter->drawLine(realP1, realP2);
    }
}

void KisHandlePainterHelper::drawPath(const PkPainterPath &path)
{
    const PkPainterPath realPath = m_painterTransform.map(path);

    for (KisHandleStyle::IterationStyle it : m_handleStyle.lineIterations) {
        it.stylePair.first.setWidthF(it.stylePair.first.widthF() * m_decorationThickness);
        PenBrushSaver saver(it.isValid ? m_painter : 0, it.stylePair, PenBrushSaver::allow_noop);
        m_painter->drawPath(realPath);
    }
}

void KisHandlePainterHelper::drawPixmap(const PkPixmap &pixmap, PkPointF position, int size, PkRectF sourceRect)
{
    PkPointF handlePolygon = m_painterTransform.map(position);

    PkPoint offsetPosition(0, 40);
    handlePolygon += offsetPosition;

    handlePolygon -= PkPointF(size*0.5,size*0.5);

    m_painter->drawPixmap(PkRect(handlePolygon.x(), handlePolygon.y(),
                                size, size),
                                pixmap,
                                sourceRect);
}

void KisHandlePainterHelper::fillHandleRect(const PkPointF &center, qreal radius, PkColor fillColor, PkPoint offset = PkPoint(0,0))
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_painter);

    PkRectF handleRect(-radius, -radius, 2 * radius, 2 * radius);
    PkPolygonF handlePolygon = m_handleTransform.map(PkPolygonF(handleRect));
    handlePolygon.translate(m_painterTransform.map(center));

    PkPainterPath painterPath;
    painterPath.addPolygon(handlePolygon);

    // offset that happens after zoom transform. This means the offset will be the same, no matter the zoom level
    // this is good for UI elements that need to be below the bounding box
    painterPath.translate(offset);

    const PkPainterPath pathToSend = painterPath;
    const PkBrush brushStyle(fillColor);
    m_painter->fillPath(pathToSend, brushStyle);
}
