/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_CANVAS_TOOL_SERVICES_H
#define KIS_CANVAS_TOOL_SERVICES_H

#include <PkFlakeBridge.h>
#include <PkObject.h>
#include <PkSignalCompat.h>
#include <PkThreadCallQueue.h>
#include <functional>
#include <KoCanvasCursorHost.h>
#include <pk/geometry/PkPoint.h>
#include <pk/geometry/PkPainterPath.h>
#include <QCursor>
#include <QObject>
#include <pk/geometry/PkRect.h>
#include <pk/geometry/PkSize.h>
#include <pk/geometry/PkTransform.h>

#include <kis_global.h>
#include <kis_types.h>
#include <kritacanvas_export.h>
#include <input/KisInputActionGroup.h>

class PkPainter;
class KisOptimizedBrushOutline;
class KisPopupWidgetInterface;

class KRITACANVAS_EXPORT KisCanvasToolSignals : public PkObject
{
public:
    using PkObject::PkObject;

signals:
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
class KRITACANVAS_EXPORT KisCanvasToolServices : public KoCanvasCursorHost
{
public:
    virtual ~KisCanvasToolServices();

    virtual KisImageWSP toolImage() const = 0;
    virtual PkPointF toolWidgetCenterInWidgetPixels() const = 0;
    virtual PkPointF toolDocumentToWidget(const PkPointF &point) const = 0;
    virtual PkPointF toolDocumentToAlignedImagePixel(const PkPointF &point) const = 0;
    virtual PkTransform toolImageToViewTransform() const = 0;
    virtual void drawToolOutline(PkPainter *painter,
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
    virtual PkSize toolCanvasWidgetSize() const = 0;
    virtual PkRect toolAvailableVirtualScreenGeometry() const = 0;
    virtual qreal toolImageScaleX() const = 0;
    virtual PkPointF toolImageToDocument(const PkPointF &point) const = 0;
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
    virtual QCursor toolForbiddenCursor() const = 0;
    virtual QCursor toolLoadCursor(const PkString &name, int hotX, int hotY) const = 0;
    QCursor loadCursorResource(const PkString &resource,
                               const PkSize &size,
                               const PkPoint &hotspot) const override
    {
        Q_UNUSED(size);
        return toolLoadCursor(resource, hotspot.x(), hotspot.y());
    }
    virtual void toolSetCursorPosition(const PkPoint &globalPoint) = 0;
    virtual void toolShowBrushSize(qreal size) = 0;
    virtual void toolShowLockedLayerMessage(bool myPaintUnavailable) = 0;
    virtual void toolShowFloatingMessage(const PkString &message,
                                         bool lockedIcon = false) = 0;
    virtual void toolShowRectangleSize(int width, int height) = 0;
    virtual void toolShowRectanglePosition(qreal x, qreal y) = 0;
    virtual PkString toolNodeEditableMessage(KisNodeSP node,
                                            bool blockedNoIndirectPainting = false) const = 0;
    virtual PkPainterPath toolShapeHoverInfoCrossLayer(const PkPointF &point,
                                                       PkString &shapeType,
                                                       bool *isHorizontal = nullptr,
                                                       bool skipCurrentShapes = true) const = 0;
    virtual bool toolSelectShapeCrossLayer(const PkPointF &point,
                                           const PkString &shapeType = PkString(),
                                           bool skipCurrentShapes = true) = 0;
    virtual void toolUpdateCanvas() = 0;
    virtual void toolSetActionCallback(const PkString &actionName,
                                       const void *receiverIdentity,
                                       PkCallLifetime receiverLifetime,
                                       std::function<void()> callback,
                                       bool unique) = 0;
    virtual void toolClearActionCallbacks(const void *receiverIdentity) = 0;
    virtual void toolSetPriorityRightClickCallback(const void *receiverIdentity,
                                                   PkCallLifetime receiverLifetime,
                                                   std::function<bool()> callback,
                                                   bool attached) = 0;
    virtual void toolSetPriorityEventFilter(QObject *filter, bool attached) = 0;
    virtual KisInputActionGroupsMaskInterface::SharedInterface
        toolInputActionGroupsMaskInterface() = 0;
    virtual void toolUpdateAssistantDecoration() = 0;
    virtual void toolUpdateOutlineDoc(const PkRectF &rect) = 0;
    virtual PkPointF toolAdjustAssistantPosition(const PkPointF &point,
                                                const PkPointF &strokeBegin,
                                                qreal magnetism,
                                                bool onlyOneAssistant,
                                                bool eraserSnap) = 0;
    virtual qreal toolAssistantPerspective(const PkPointF &documentPoint) const = 0;
    virtual void toolEndAssistantStroke() = 0;
};

#endif // KIS_CANVAS_TOOL_SERVICES_H
