/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006, 2008 Jan Hambrecht <jaham@gmx.net>
 * SPDX-FileCopyrightText: 2006, 2007 Thorsten Zachmann <zachmann@kde.org>
 * SPDX-FileCopyrightText: 2007, 2010 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2007 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QtGlobal>   // native 桶：解析到 pk/global 的垫片（<QtCore/QtCore> 是 qt 桶的 umbrella）
#include <PkFlakeBridge.h>
#include "KoPathToolHandle.h"
#include "KoPathTool.h"
#include "KoPathPointMoveStrategy.h"
#include "KoPathControlPointMoveStrategy.h"
#include "KoSelection.h"
#include "commands/KoPathPointTypeCommand.h"
#include "KoParameterChangeStrategy.h"
#include "KoParameterShape.h"
#include "KoCanvasBase.h"
#include "KoViewConverter.h"
#include "KoPointerEvent.h"
#include "KoShapeController.h"
#include <PkPainter.h>
#include <KisHandlePainterHelper.h>


KoPathToolHandle::KoPathToolHandle(KoPathTool *tool)
        : m_tool(tool)
{
}

KoPathToolHandle::~KoPathToolHandle()
{
}


PointHandle::PointHandle(KoPathTool *tool, KoPathPoint *activePoint, KoPathPoint::PointType activePointType)
        : KoPathToolHandle(tool)
        , m_activePoint(activePoint)
        , m_activePointType(activePointType)
{
}

void PointHandle::paint(PkPainter &painter, const KoViewConverter &converter, qreal handleRadius, int decorationThickness)
{
    KoPathToolSelection * selection = dynamic_cast<KoPathToolSelection*>(m_tool->selection());

    KoPathPoint::PointTypes allPaintedTypes = KoPathPoint::Node;
    if (selection && selection->contains(m_activePoint)) {
        allPaintedTypes = KoPathPoint::All;
    }


    KisHandlePainterHelper helper = KoShape::createHandlePainterHelperView(&painter, m_activePoint->parent(), converter, handleRadius, decorationThickness);


    if (allPaintedTypes != m_activePointType) {
        KoPathPoint::PointTypes nonHighlightedType = allPaintedTypes & ~m_activePointType;
        KoPathPoint::PointTypes nonNodeType = nonHighlightedType & ~KoPathPoint::Node;

        if (nonNodeType != KoPathPoint::None) {
            helper.setHandleStyle(KisHandleStyle::selectedPrimaryHandles());
            m_activePoint->paint(helper, nonHighlightedType);
        }

        if (nonHighlightedType & KoPathPoint::Node) {
            helper.setHandleStyle(KisHandleStyle::partiallyHighlightedPrimaryHandles());
            m_activePoint->paint(helper, KoPathPoint::Node);
        }
    }

    helper.setHandleStyle(KisHandleStyle::highlightedPrimaryHandles());
    m_activePoint->paint(helper, m_activePointType);
}

PkRectF PointHandle::boundingRect() const
{
    bool active = false;
    KoPathToolSelection * selection = dynamic_cast<KoPathToolSelection*>(m_tool->selection());
    if (selection && selection->contains(m_activePoint))
        active = true;
    return m_activePoint->boundingRect(!active);
}

KoInteractionStrategy * PointHandle::handleMousePress(KoPointerEvent *event)
{
    if ((event->button() & Pk::LeftButton) == 0)
        return 0;
    if ((event->modifiers() & Pk::ControlModifier) == 0) { // no shift pressed.
        KoPathToolSelection * selection = dynamic_cast<KoPathToolSelection*>(m_tool->selection());
        KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(selection, 0);

        // control select adds/removes points to/from the selection
        if (event->modifiers() & Pk::ShiftModifier) {
            if (selection->contains(m_activePoint)) {
                selection->remove(m_activePoint);
            } else {
                selection->add(m_activePoint, false);
            }
        } else {
            // no control modifier, so clear selection and select active point
            if (!selection->contains(m_activePoint)) {
                selection->add(m_activePoint, true);
            }
        }
        // TODO remove canvas from call ?
        if (m_activePointType == KoPathPoint::Node) {
            PkPointF movedPointPosition = m_activePoint->parent()->shapeToDocument(m_activePoint->point());
            return new KoPathPointMoveStrategy(m_tool, event->point, movedPointPosition);
        } else {
            KoPathShape * pathShape = m_activePoint->parent();
            KoPathPointData pd(pathShape, pathShape->pathPointIndex(m_activePoint));
            return new KoPathControlPointMoveStrategy(m_tool, pd, m_activePointType, event->point);
        }
    } else {
        KoPathPoint::PointProperties props = m_activePoint->properties();
        if (! m_activePoint->activeControlPoint1() || ! m_activePoint->activeControlPoint2())
            return 0;

        KoPathPointTypeCommand::PointType pointType = KoPathPointTypeCommand::Smooth;
        // cycle the smooth->symmetric->unsmooth state of the path point
        if (props & KoPathPoint::IsSmooth)
            pointType = KoPathPointTypeCommand::Symmetric;
        else if (props & KoPathPoint::IsSymmetric)
            pointType = KoPathPointTypeCommand::Corner;

        PkList<KoPathPointData> pointData;
        pointData.append(KoPathPointData(m_activePoint->parent(), m_activePoint->parent()->pathPointIndex(m_activePoint)));
        m_tool->canvas()->addCommand(new KoPathPointTypeCommand(toPkList(pointData), pointType));
    }
    return 0;
}

bool PointHandle::check(const PkList<KoPathShape*> &selectedShapes)
{
    if (selectedShapes.contains(m_activePoint->parent())) {
        return m_activePoint->parent()->pathPointIndex(m_activePoint) != KoPathPointIndex(-1, -1);
    }
    return false;
}

KoPathPoint * PointHandle::activePoint() const
{
    return m_activePoint;
}

KoPathPoint::PointType PointHandle::activePointType() const
{
    return m_activePointType;
}

void PointHandle::trySelectHandle()
{
    KoPathToolSelection * selection = dynamic_cast<KoPathToolSelection*>(m_tool->selection());
    KIS_SAFE_ASSERT_RECOVER_RETURN(selection);

    if (!selection->contains(m_activePoint) && m_activePointType == KoPathPoint::Node) {
        selection->clear();
        selection->add(m_activePoint, false);
    }
}

ParameterHandle::ParameterHandle(KoPathTool *tool, KoParameterShape *parameterShape, int handleId)
        : KoPathToolHandle(tool)
        , m_parameterShape(parameterShape)
        , m_handleId(handleId)
{
}

void ParameterHandle::paint(PkPainter &painter, const KoViewConverter &converter, qreal handleRadius, int decorationThickness)
{
    KisHandlePainterHelper helper = KoShape::createHandlePainterHelperView(&painter, m_parameterShape, converter, handleRadius, decorationThickness);
    helper.setHandleStyle(KisHandleStyle::highlightedPrimaryHandles());
    m_parameterShape->paintHandle(helper, m_handleId);
}

PkRectF ParameterHandle::boundingRect() const
{
    return m_parameterShape->shapeToDocument(PkRectF(m_parameterShape->handlePosition(m_handleId), PkSize(1, 1)));
}

KoInteractionStrategy * ParameterHandle::handleMousePress(KoPointerEvent *event)
{
    if (event->button() & Pk::LeftButton) {
        KoPathToolSelection * selection = dynamic_cast<KoPathToolSelection*>(m_tool->selection());
        if (selection)
            selection->clear();
        return new KoParameterChangeStrategy(m_tool, m_parameterShape, m_handleId);
    }
    return 0;
}

bool ParameterHandle::check(const PkList<KoPathShape*> &selectedShapes)
{
    return selectedShapes.contains(m_parameterShape);
}
