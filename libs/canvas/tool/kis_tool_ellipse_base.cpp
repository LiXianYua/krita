/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2009 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <PkFlakeBridge.h>

#include "kis_tool_ellipse_base.h"

#include <KoPointerEvent.h>
#include <KoCanvasBase.h>
#include <KoCanvasController.h>
#include <KoViewConverter.h>
#include <KisOptimizedBrushOutline.h>
KisToolEllipseBase::KisToolEllipseBase(KoCanvasBase * canvas, KisToolEllipseBase::ToolType type, const QCursor & cursor)
    : KisToolRectangleBase(canvas, type, cursor)
{
}

void KisToolEllipseBase::paintRectangle(PkPainter &gc, const PkRectF &imageRect)
{
    KIS_ASSERT_RECOVER_RETURN(canvas());

    const PkRectF viewRect = pixelToView(imageRect);

    PkPainterPath path;
    path.addEllipse(viewRect);
    getRotatedPath(path, viewRect.center(), getRotationAngle());
    path.addPath(drawX(pixelToView(m_dragStart)));
    path.addPath(drawX(pixelToView(m_dragCenter)));
    paintToolOutline(&gc, KisOptimizedBrushOutline(path));
}

bool KisToolEllipseBase::showRoundCornersGUI() const
{
    return false;
}
