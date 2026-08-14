/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2009 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KIS_TOOL_ELLIPSE_BASE_H
#define KIS_TOOL_ELLIPSE_BASE_H

#include <kis_tool_rectangle_base.h>

class KRITACANVAS_EXPORT KisToolEllipseBase : public KisToolRectangleBase
{
public:
    KisToolEllipseBase(KoCanvasBase * canvas, KisToolEllipseBase::ToolType type, const QCursor & cursor);

    void paintRectangle(QPainter &gc, const QRectF &imageRect) override;

protected:
    bool showRoundCornersGUI() const override;
};

#endif // KIS_TOOL_ELLIPSE_BASE_H
