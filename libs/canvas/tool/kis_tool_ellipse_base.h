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
    KisToolEllipseBase(KoCanvasBase * canvas, KisToolEllipseBase::ToolType type, KisCanvasCursorToken cursor);

    void paintRectangle(PkPainter &gc, const PkRectF &imageRect) override;

protected:
    bool showRoundCornersGUI() const override;
};

#endif // KIS_TOOL_ELLIPSE_BASE_H
