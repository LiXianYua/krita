/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2009 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <PkPainterPath.h>
#include <PkPen.h>
#include <PkFlakeBridge.h>

#include "kis_tool_ellipse_base.h"

#include <KoPointerEvent.h>
#include <KoCanvasBase.h>
#include <KoCanvasController.h>
#include <KoViewConverter.h>
#include <KisOptimizedBrushOutline.h>
KisToolEllipseBase::KisToolEllipseBase(KoCanvasBase * canvas, KisToolEllipseBase::ToolType type, KisCanvasCursorToken cursor)
    : KisToolRectangleBase(canvas, type, cursor)
{
}

void KisToolEllipseBase::paintRectangle(PkPainter &gc, const PkRectF &imageRect)
{
    KIS_ASSERT_RECOVER_RETURN(canvas());

    const PkRect viewRect = pixelToView(imageRect).toRect();

    PkPainterPath path;
    path.addEllipse(PkRectF(viewRect));
    getRotatedPath(path, PkPointF(viewRect.center()), getRotationAngle());
    path.addPath(drawX(pixelToView(m_dragStart)));
    path.addPath(drawX(pixelToView(m_dragCenter)));
    paintToolOutline(&gc, KisOptimizedBrushOutline(path));
}

bool KisToolEllipseBase::showRoundCornersGUI() const
{
    return false;
}
