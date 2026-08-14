/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_CANVAS_TOOL_SERVICES_H
#define KIS_CANVAS_TOOL_SERVICES_H

#include <QPointF>
#include <QTransform>

#include <kis_types.h>
#include <kritacanvas_export.h>

class QPainter;
class KisOptimizedBrushOutline;

/**
 * Narrow canvas services required by the retained KisTool base.
 *
 * Tool algorithms need image and coordinate access plus a few host-mediated
 * operations, but must not depend on the desktop canvas implementation.
 */
class KRITACANVAS_EXPORT KisCanvasToolServices
{
public:
    virtual ~KisCanvasToolServices();

    virtual KisImageWSP toolImage() const = 0;
    virtual QPointF toolWidgetCenterInWidgetPixels() const = 0;
    virtual QPointF toolDocumentToWidget(const QPointF &point) const = 0;
    virtual QPointF toolDocumentToAlignedImagePixel(const QPointF &point) const = 0;
    virtual QTransform toolImageToViewTransform() const = 0;
    virtual void drawToolOutline(QPainter *painter,
                                 const KisOptimizedBrushOutline &path,
                                 int thickness) = 0;
    virtual bool toolBlockUntilOperationsFinished(KisImageWSP image) = 0;
    virtual void toolBlockUntilOperationsFinishedForced(KisImageWSP image) = 0;
    virtual bool toolSelectionEditable() const = 0;
};

#endif // KIS_CANVAS_TOOL_SERVICES_H
