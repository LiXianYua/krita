/*
 *  kis_tool_polygon.cc -- part of Krita
 *
 *  SPDX-FileCopyrightText: 2004 Michael Thaler <michael.thaler@physik.tu-muenchen.de>
 *  SPDX-FileCopyrightText: 2009 Lukáš Tvrdý <lukast.dev@gmail.com>
 *  SPDX-FileCopyrightText: 2010 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2010 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_tool_polygon.h"

// QCursor：native 桶的桶无关不透明句柄由 libs/flake/flake/noqt-compat/QCursor 提供
// （规格 2026-09-10 裁定：QCursor 走 KoCanvasCursorHost 句柄载体）。
// <QCursor> 是私有 TU 的 include，不是公开头（约束 9 只管公开头）；
// 先例：plugins/tools/tool_enclose_and_fill/subtools/KisToolBasicBrushBase.cpp:8。
#include <QCursor>

#include <PkTransform.h>

#include <KoPointerEvent.h>
#include <KoCanvasBase.h>
#include <KoPathShape.h>
#include <KoShapeStroke.h>

#include <brushengine/kis_paintop_registry.h>
#include <KisCanvasToolServices.h>
#include "kis_figure_painting_tool_helper.h"

KisToolPolygon::KisToolPolygon(KoCanvasBase *canvas)
        : KisToolPolylineBase(canvas, KisToolPolylineBase::PAINT,
                              dynamic_cast<KisCanvasToolServices*>(canvas)->toolLoadCursorToken("tool_polygon_cursor.png", 6, 6))
{
    PkObject::setObjectName("tool_polygon");
    setSupportOutline(true);
    setIsOpacityPresetMode(true);
}

KisToolPolygon::~KisToolPolygon()
{
}

void KisToolPolygon::resetCursorStyle()
{
    if (isEraser()) {
        useCursor(dynamic_cast<KisCanvasToolServices*>(canvas())->toolLoadCursorToken("tool_polygon_eraser_cursor.png", 6, 6));
    } else {
        KisToolPolylineBase::resetCursorStyle();
    }

    overrideCursorIfNotEditable();
}

void KisToolPolygon::finishPolyline(const PkVector<PkPointF> &points)
{
    PkVector<PkPointF> pkPoints;
    pkPoints.reserve(points.size());
    for (const PkPointF &point : points) {
        pkPoints.append(point);
    }
    finishPolylinePk(pkPoints);
}

void KisToolPolygon::finishPolylinePk(const PkVector<PkPointF>& points)
{
    const KisToolShape::ShapeAddInfo info =
        shouldAddShape(currentNode());

    if (!info.shouldAddShape) {
        KisFigurePaintingToolHelper helper(kundo2_text("Draw Polygon"),
                                           image(),
                                           currentNode(),
                                           canvas()->resourceManager()->canvasResourcesInterface(),
                                           strokeStyle(),
                                           fillStyle(),
                                           fillTransform());
        helper.paintPolygon(points);
    } else {
        // remove the last point if it overlaps with the first
        PkVector<PkPointF> newPoints = points;
        if (newPoints.size() > 1 && newPoints.first() == newPoints.last()) {
            newPoints.remove(newPoints.size() - 1);
        }
        KoPathShape* path = new KoPathShape();
        path->setShapeId(KoPathShapeId);

        PkTransform resolutionMatrix;
        resolutionMatrix.scale(1 / currentImage()->xRes(), 1 / currentImage()->yRes());
        path->moveTo(resolutionMatrix.map(newPoints[0]));
        for (int i = 1; i < newPoints.size(); i++)
            path->lineTo(resolutionMatrix.map(newPoints[i]));
        path->close();
        path->normalize();

        info.markAsSelectionShapeIfNeeded(path);

        addShape(path);
    }
}

bool KisToolPolygon::supportsPaintingAssistants() const
{
    return true;
}

KoToolBase *KisToolPolygonFactory::createTool(KoCanvasBase *canvas)
{
    return new KisToolPolygon(canvas);
}
