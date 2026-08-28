/* This file is part of the KDE project
   SPDX-FileCopyrightText: 2006-2008 Thorsten Zachmann <zachmann@kde.org>
   SPDX-FileCopyrightText: 2006, 2008 Jan Hambrecht <jaham@gmx.net>
   SPDX-FileCopyrightText: 2009 Thomas Zander <zander@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "EllipseShape.h"

#include <KoPathPoint.h>
#include <KoShapeSavingContext.h>
#include <KoXmlWriter.h>
#include <KoXmlNS.h>
#include <KoUnit.h>
#include <SvgSavingContext.h>
#include <SvgLoadingContext.h>
#include <SvgUtil.h>
#include <SvgStyleWriter.h>

#include <KoParameterShape_p.h>
#include "kis_global.h"

#include <cmath>

EllipseShape::EllipseShape()
    : m_startAngle(0)
    , m_endAngle(0)
    , m_kindAngle(M_PI)
    , m_type(Arc)
{
    PkList<PkPointF> handles;
    handles.push_back(PkPointF(100, 50));
    handles.push_back(PkPointF(100, 50));
    handles.push_back(PkPointF(0, 50));
    setHandles(handles);
    PkSizeF size(100, 100);
    m_radii = PkPointF(size.width() / 2.0, size.height() / 2.0);
    m_center = PkPointF(m_radii.x(), m_radii.y());
    updatePath(size);
}

EllipseShape::EllipseShape(const EllipseShape &rhs)
    : KoParameterShape(rhs),
      m_startAngle(rhs.m_startAngle),
      m_endAngle(rhs.m_endAngle),
      m_kindAngle(rhs.m_kindAngle),
      m_center(rhs.m_center),
      m_radii(rhs.m_radii),
      m_type(rhs.m_type)
{
}

EllipseShape::~EllipseShape()
{
}

KoShape *EllipseShape::cloneShape() const
{
    return new EllipseShape(*this);
}

void EllipseShape::setSize(const PkSizeF &newSize)
{
    PkTransform matrix(resizeMatrix(newSize));
    m_center = matrix.map(m_center);
    m_radii = matrix.map(m_radii);
    KoParameterShape::setSize(newSize);
}

PkPointF EllipseShape::normalize()
{
    PkPointF offset(KoParameterShape::normalize());
    PkTransform matrix;
    matrix.translate(-offset.x(), -offset.y());
    m_center = matrix.map(m_center);
    return offset;
}

void EllipseShape::moveHandleAction(int handleId, const PkPointF &point, Qt::KeyboardModifiers modifiers)
{
    (void)modifiers;
    PkPointF p(point);

    PkPointF diff(m_center - point);
    diff.setX(-diff.x());
    qreal angle = 0;
    if (diff.x() == 0) {
        angle = (diff.y() < 0 ? 270 : 90) * M_PI / 180.0;
    } else {
        diff.setY(diff.y() * m_radii.x() / m_radii.y());
        angle = atan(diff.y() / diff.x());
        if (angle < 0) {
            angle += M_PI;
        }

        if (diff.y() < 0) {
            angle += M_PI;
        }
    }

    PkList<PkPointF> handles = this->handles();
    switch (handleId) {
    case 0:
        p = PkPointF(m_center + PkPointF(cos(angle) * m_radii.x(), -sin(angle) * m_radii.y()));
        m_startAngle = kisRadiansToDegrees(angle);
        handles[handleId] = p;
        break;
    case 1:
        p = PkPointF(m_center + PkPointF(cos(angle) * m_radii.x(), -sin(angle) * m_radii.y()));
        m_endAngle = kisRadiansToDegrees(angle);
        handles[handleId] = p;
        break;
    case 2: {
        PkList<PkPointF> kindHandlePositions;
        kindHandlePositions.push_back(PkPointF(m_center + PkPointF(cos(m_kindAngle) * m_radii.x(), -sin(m_kindAngle) * m_radii.y())));
        kindHandlePositions.push_back(m_center);
        kindHandlePositions.push_back((handles[0] + handles[1]) / 2.0);

        PkPointF diff = m_center * 2.0;
        int handlePos = 0;
        for (int i = 0; i < kindHandlePositions.size(); ++i) {
            PkPointF pointDiff(p - kindHandlePositions[i]);
            if (i == 0 || std::abs(pointDiff.x()) + std::abs(pointDiff.y()) < std::abs(diff.x()) + std::abs(diff.y())) {
                diff = pointDiff;
                handlePos = i;
            }
        }
        handles[handleId] = kindHandlePositions[handlePos];
        m_type = EllipseType(handlePos);
    }
    break;
    }
    setHandles(handles);

    if (handleId != 2) {
        updateKindHandle();
    }
}

void EllipseShape::updatePath(const PkSizeF &size)
{
    (void)size;
    PkPointF startpoint(handles()[0]);

    PkPointF curvePoints[12];
    const qreal distance = sweepAngle();

    const bool sameAngles = distance > 359.9;
    int pointCnt = arcToCurve(m_radii.x(), m_radii.y(), m_startAngle, distance, startpoint, curvePoints);
    KIS_SAFE_ASSERT_RECOVER_RETURN(pointCnt);

    int curvePointCount = 1 + pointCnt / 3;
    int requiredPointCount = curvePointCount;
    if (m_type == Pie) {
        requiredPointCount++;
    } else if (m_type == Arc && sameAngles) {
        curvePointCount--;
        requiredPointCount--;
    }

    createPoints(requiredPointCount);

    KoSubpath &points = *subpaths()[0];

    int curveIndex = 0;
    points[0]->setPoint(startpoint);
    points[0]->removeControlPoint1();
    points[0]->setProperty(KoPathPoint::StartSubpath);
    for (int i = 1; i < curvePointCount; ++i) {
        points[i - 1]->setControlPoint2(curvePoints[curveIndex++]);
        points[i]->setControlPoint1(curvePoints[curveIndex++]);
        points[i]->setPoint(curvePoints[curveIndex++]);
        points[i]->removeControlPoint2();
    }

    if (m_type == Pie) {
        points[requiredPointCount - 1]->setPoint(m_center);
        points[requiredPointCount - 1]->removeControlPoint1();
        points[requiredPointCount - 1]->removeControlPoint2();
    } else if (m_type == Arc && sameAngles) {
        points[curvePointCount - 1]->setControlPoint2(curvePoints[curveIndex]);
        points[0]->setControlPoint1(curvePoints[++curveIndex]);
    }

    for (int i = 0; i < requiredPointCount; ++i) {
        points[i]->unsetProperty(KoPathPoint::StopSubpath);
        points[i]->unsetProperty(KoPathPoint::CloseSubpath);
    }
    subpaths()[0]->last()->setProperty(KoPathPoint::StopSubpath);
    if (m_type == Arc && !sameAngles) {
        subpaths()[0]->first()->unsetProperty(KoPathPoint::CloseSubpath);
        subpaths()[0]->last()->unsetProperty(KoPathPoint::CloseSubpath);
    } else {
        subpaths()[0]->first()->setProperty(KoPathPoint::CloseSubpath);
        subpaths()[0]->last()->setProperty(KoPathPoint::CloseSubpath);
    }

    notifyPointsChanged();

    normalize();
}

void EllipseShape::createPoints(int requiredPointCount)
{
    if (subpaths().count() != 1) {
        clear();
        subpaths().append(new KoSubpath());
    }
    int currentPointCount = subpaths()[0]->count();
    if (currentPointCount > requiredPointCount) {
        for (int i = 0; i < currentPointCount - requiredPointCount; ++i) {
            delete subpaths()[0]->front();
            subpaths()[0]->pop_front();
        }
    } else if (requiredPointCount > currentPointCount) {
        for (int i = 0; i < requiredPointCount - currentPointCount; ++i) {
            subpaths()[0]->append(new KoPathPoint(this, PkPointF()));
        }
    }

    notifyPointsChanged();
}

void EllipseShape::updateKindHandle()
{
    qreal angle = 0.5 * (m_startAngle + m_endAngle);
    if (m_startAngle > m_endAngle) {
        angle += 180.0;
    }

    m_kindAngle = normalizeAngle(kisDegreesToRadians(angle));

    PkList<PkPointF> handles = this->handles();
    switch (m_type) {
    case Arc:
        handles[2] = m_center + PkPointF(cos(m_kindAngle) * m_radii.x(), -sin(m_kindAngle) * m_radii.y());
        break;
    case Pie:
        handles[2] = m_center;
        break;
    case Chord:
        handles[2] = (handles[0] + handles[1]) / 2.0;
        break;
    }
    setHandles(handles);
}

void EllipseShape::updateAngleHandles()
{
    qreal startRadian = kisDegreesToRadians(normalizeAngleDegrees(m_startAngle));
    qreal endRadian = kisDegreesToRadians(normalizeAngleDegrees(m_endAngle));
    PkList<PkPointF> handles = this->handles();
    handles[0] = m_center + PkPointF(cos(startRadian) * m_radii.x(), -sin(startRadian) * m_radii.y());
    handles[1] = m_center + PkPointF(cos(endRadian) * m_radii.x(), -sin(endRadian) * m_radii.y());
    setHandles(handles);
}

qreal EllipseShape::sweepAngle() const
{
    const qreal a1 = normalizeAngle(kisDegreesToRadians(m_startAngle));
    const qreal a2 = normalizeAngle(kisDegreesToRadians(m_endAngle));

    qreal sAngle = a2 - a1;

    if (a1 > a2) {
        sAngle = 2 * M_PI + sAngle;
    }

    if (std::abs(a1 - a2) < 0.05 / M_PI) {
        sAngle = 2 * M_PI;
    }

    return kisRadiansToDegrees(sAngle);
}

void EllipseShape::setType(EllipseType type)
{
    m_type = type;
    updateKindHandle();
    updatePath(size());
}

EllipseShape::EllipseType EllipseShape::type() const
{
    return m_type;
}

void EllipseShape::setStartAngle(qreal angle)
{
    m_startAngle = angle;
    updateKindHandle();
    updateAngleHandles();
    updatePath(size());
}

qreal EllipseShape::startAngle() const
{
    return m_startAngle;
}

void EllipseShape::setEndAngle(qreal angle)
{
    m_endAngle = angle;
    updateKindHandle();
    updateAngleHandles();
    updatePath(size());
}

qreal EllipseShape::endAngle() const
{
    return m_endAngle;
}

PkString EllipseShape::pathShapeId() const
{
    return EllipseShapeId;
}

bool EllipseShape::saveSvg(SvgSavingContext &context)
{
    // let basic path saiving code handle our saving
    if (!isParametricShape()) return false;

    if (type() == EllipseShape::Arc && startAngle() == endAngle()) {
        const PkSizeF size = this->size();
        const bool isCircle = size.width() == size.height();
        context.shapeWriter().startElement(isCircle ? "circle" : "ellipse");
        context.shapeWriter().addAttribute("id", context.getID(this));
        SvgUtil::writeTransformAttributeLazy("transform", transformation(), context.shapeWriter());
        SvgStyleWriter::saveMetadata(this, context);

        if (isCircle) {
            context.shapeWriter().addAttribute("r", 0.5 * size.width());
        } else {
            context.shapeWriter().addAttribute("rx", 0.5 * size.width());
            context.shapeWriter().addAttribute("ry", 0.5 * size.height());
        }
        context.shapeWriter().addAttribute("cx", 0.5 * size.width());
        context.shapeWriter().addAttribute("cy", 0.5 * size.height());

        SvgStyleWriter::saveSvgStyle(this, context);

        context.shapeWriter().endElement();
    } else {
        context.shapeWriter().startElement("path");
        context.shapeWriter().addAttribute("id", context.getID(this));
        SvgUtil::writeTransformAttributeLazy("transform", transformation(), context.shapeWriter());

        context.shapeWriter().addAttribute("sodipodi:type", "arc");

        context.shapeWriter().addAttribute("sodipodi:rx", m_radii.x());
        context.shapeWriter().addAttribute("sodipodi:ry", m_radii.y());

        context.shapeWriter().addAttribute("sodipodi:cx", m_center.x());
        context.shapeWriter().addAttribute("sodipodi:cy", m_center.y());

        context.shapeWriter().addAttribute("sodipodi:start", 2 * M_PI - kisDegreesToRadians(endAngle()));
        context.shapeWriter().addAttribute("sodipodi:end", 2 * M_PI - kisDegreesToRadians(startAngle()));

        switch (type()) {
        case Pie:
            // noop
            break;
        case Chord:
            context.shapeWriter().addAttribute("sodipodi:arc-type", "chord");
            break;
        case Arc:
            context.shapeWriter().addAttribute("sodipodi:open", "true");
            break;
        }

        context.shapeWriter().addAttribute("d", this->toString(context.userSpaceTransform()));

        SvgStyleWriter::saveSvgStyle(this, context);

        context.shapeWriter().endElement();
    }

    return true;
}

bool EllipseShape::loadSvg(const PkXmlElement &element, SvgLoadingContext &context)
{
    qreal rx = 0, ry = 0;
    qreal cx = 0;
    qreal cy = 0;
    qreal start = 0;
    qreal end = 0;
    EllipseType type = Arc;

    const PkString extendedNamespace =
            element.attribute("sodipodi:type") == "arc" ? "sodipodi" :
            element.attribute("krita:type") == "arc" ? "krita" : "";

    if (element.tagName() == "ellipse") {
        rx = SvgUtil::parseUnitX(context.currentGC(), context.resolvedProperties(), element.attribute("rx"));
        ry = SvgUtil::parseUnitY(context.currentGC(), context.resolvedProperties(), element.attribute("ry"));
        cx = SvgUtil::parseUnitX(context.currentGC(), context.resolvedProperties(), element.attribute("cx", "0"));
        cy = SvgUtil::parseUnitY(context.currentGC(), context.resolvedProperties(), element.attribute("cy", "0"));
    } else if (element.tagName() == "circle") {
        rx = ry = SvgUtil::parseUnitXY(context.currentGC(), context.resolvedProperties(), element.attribute("r"));
        cx = SvgUtil::parseUnitX(context.currentGC(), context.resolvedProperties(), element.attribute("cx", "0"));
        cy = SvgUtil::parseUnitY(context.currentGC(), context.resolvedProperties(), element.attribute("cy", "0"));

    } else if (element.tagName() == "path" && !extendedNamespace.isEmpty()) {
        rx = SvgUtil::parseUnitX(context.currentGC(), context.resolvedProperties(), element.attribute(extendedNamespace + ":rx"));
        ry = SvgUtil::parseUnitY(context.currentGC(), context.resolvedProperties(), element.attribute(extendedNamespace + ":ry"));
        cx = SvgUtil::parseUnitX(context.currentGC(), context.resolvedProperties(), element.attribute(extendedNamespace + ":cx", "0"));
        cy = SvgUtil::parseUnitY(context.currentGC(), context.resolvedProperties(), element.attribute(extendedNamespace + ":cy", "0"));
        start = 2 * M_PI - SvgUtil::parseNumber(element.attribute(extendedNamespace + ":end"));
        end = 2 * M_PI - SvgUtil::parseNumber(element.attribute(extendedNamespace + ":start"));

        const PkString kritaArcType =
            element.attribute("sodipodi:arc-type", element.attribute("krita:arcType"));

        if (kritaArcType.isEmpty()) {
            if (element.attribute("sodipodi:open", "false") == "false") {
                type = Pie;
            }
        } else if (kritaArcType == "pie") {
            type = Pie;
        } else if (kritaArcType == "chord") {
            type = Chord;
        }
    } else {
        return false;
    }

    setSize(PkSizeF(2 * rx, 2 * ry));
    setPosition(PkPointF(cx - rx, cy - ry));
    if (rx == 0.0 || ry == 0.0) {
        setVisible(false);
    }

    if (start != 0 || start != end) {
        setStartAngle(kisRadiansToDegrees(start));
        setEndAngle(kisRadiansToDegrees(end));
        setType(type);
    }

    return true;
}
