/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_CANVAS_TOOL_SERVICES_H
#define KIS_CANVAS_TOOL_SERVICES_H

#include <QPointF>
#include <QPainterPath>
#include <QCursor>
#include <QObject>
#include <QRect>
#include <QSize>
#include <QTransform>

#include <kis_global.h>
#include <kis_types.h>
#include <kritacanvas_export.h>
#include <input/KisInputActionGroup.h>

class QPainter;
class KisOptimizedBrushOutline;
class KisPopupWidgetInterface;

class KRITACANVAS_EXPORT KisCanvasToolSignals : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

Q_SIGNALS:
    void brushOutlineChanged();
    void effectiveCompositeOpChanged();
    void paintOpPresetChanged();
};

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

    virtual KisCanvasToolSignals *toolSignals() = 0;
    virtual KisPaintOpPresetSP toolCurrentPaintOpPreset() const = 0;
    virtual void toolNotifyPaintingFinished() = 0;
    virtual void toolSetControlsEnabled(bool enabled) = 0;
    virtual KisPopupWidgetInterface *toolPopupWidget() const = 0;
    virtual QSize toolCanvasWidgetSize() const = 0;
    virtual QRect toolAvailableVirtualScreenGeometry() const = 0;
    virtual qreal toolImageScaleX() const = 0;
    virtual QPointF toolImageToDocument(const QPointF &point) const = 0;
    virtual qreal toolCanvasRotation() const = 0;
    virtual bool toolCanvasMirroredHorizontally() const = 0;
    virtual bool toolCanvasMirroredVertically() const = 0;
    virtual qreal toolEffectiveZoom() const = 0;
    virtual qreal toolCoordinateEffectiveZoom() const = 0;
    virtual qreal toolEffectivePhysicalZoom() const = 0;
    virtual QCursor toolCursor(CursorStyle style) const = 0;
    virtual QCursor toolMoveCursor() const = 0;
    virtual QCursor toolMoveSelectionCursor() const = 0;
    virtual QCursor toolSamplerCursor() const = 0;
    virtual QCursor toolOpenHandCursor() const = 0;
    virtual QCursor toolClosedHandCursor() const = 0;
    virtual QCursor toolLoadCursor(const QString &name, int hotX, int hotY) const = 0;
    virtual void toolSetCursorPosition(const QPoint &globalPoint) = 0;
    virtual void toolShowBrushSize(qreal size) = 0;
    virtual void toolShowLockedLayerMessage(bool myPaintUnavailable) = 0;
    virtual void toolShowFloatingMessage(const QString &message,
                                         bool lockedIcon = false) = 0;
    virtual QString toolNodeEditableMessage(KisNodeSP node,
                                            bool blockedNoIndirectPainting = false) const = 0;
    virtual QPainterPath toolShapeHoverInfoCrossLayer(const QPointF &point,
                                                      QString &shapeType,
                                                      bool *isHorizontal = nullptr,
                                                      bool skipCurrentShapes = true) const = 0;
    virtual bool toolSelectShapeCrossLayer(const QPointF &point,
                                           const QString &shapeType = QString(),
                                           bool skipCurrentShapes = true) = 0;
    virtual void toolUpdateCanvas() = 0;
    virtual void toolSetPriorityEventFilter(QObject *filter, bool attached) = 0;
    virtual KisInputActionGroupsMaskInterface::SharedInterface
        toolInputActionGroupsMaskInterface() = 0;
    virtual void toolUpdateAssistantDecoration() = 0;
    virtual void toolUpdateOutlineDoc(const QRectF &rect) = 0;
    virtual QPointF toolAdjustAssistantPosition(const QPointF &point,
                                                const QPointF &strokeBegin,
                                                qreal magnetism,
                                                bool onlyOneAssistant,
                                                bool eraserSnap) = 0;
    virtual qreal toolAssistantPerspective(const QPointF &documentPoint) const = 0;
    virtual void toolEndAssistantStroke() = 0;
};

#endif // KIS_CANVAS_TOOL_SERVICES_H
