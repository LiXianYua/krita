/*
 *  SPDX-FileCopyrightText: 2011 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_FIGURE_PAINTING_TOOL_HELPER_H
#define __KIS_FIGURE_PAINTING_TOOL_HELPER_H

#include "kis_types.h"
#include <kritapaintop_export.h>
#include <KoCanvasResourcesInterface.h>
#include <brushengine/kis_paint_information.h>
#include "strokes/freehand_stroke.h"
#include "KisToolShapeUtils.h"

class KisStrokesFacade;

class PAINTOP_EXPORT KisFigurePaintingToolHelper
{
public:
    KisFigurePaintingToolHelper(const KUndo2MagicString &name,
                                KisImageWSP image,
                                KisNodeSP currentNode,
                                KoCanvasResourcesInterfaceSP canvasResources,
                                KisToolShapeUtils::StrokeStyle strokeStyle,
                                KisToolShapeUtils::FillStyle fillStyle,
                                PkTransform fillTransform = PkTransform());
    ~KisFigurePaintingToolHelper();

    void paintLine(const KisPaintInformation &pi0,
                   const KisPaintInformation &pi1);
    void paintPolyline(const vQPointF &points);
    void paintPolygon(const vQPointF &points);
    void paintRect(const PkRectF &rect);
    void paintEllipse(const PkRectF &rect);
    void paintPainterPath(const PkPainterPath &path);
    void setFGColorOverride(const KoColor &color);
    void setBGColorOverride(const KoColor &color);
    void setSelectionOverride(KisSelectionSP m_selection);
    void setBrush(const KisPaintOpPresetSP &brush);
    void paintPainterPathQPen(const PkPainterPath, const PkPen &pen, const KoColor &color);
    void paintPainterPathQPenFill(const PkPainterPath, const PkPen &pen, const KoColor &color);

private:
    void setupPaintStyles(KisResourcesSnapshotSP resources,
                          KisToolShapeUtils::StrokeStyle strokeStyle,
                          KisToolShapeUtils::FillStyle fillStyle,
                          PkTransform fillTransform);

private:
    KisStrokeId m_strokeId;
    KisResourcesSnapshotSP m_resources;
    KisStrokesFacade *m_strokesFacade;
};

#endif /* __KIS_FIGURE_PAINTING_TOOL_HELPER_H */
