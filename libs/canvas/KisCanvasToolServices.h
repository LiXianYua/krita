/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_CANVAS_TOOL_SERVICES_H
#define KIS_CANVAS_TOOL_SERVICES_H

#include <PkPainterPath.h>
#include <PkPen.h>
#include <PkFlakeBridge.h>
#include <PkObject.h>
#include <PkSignalCompat.h>
#include <PkThreadCallQueue.h>
#include <functional>
#include <KoCanvasCursorHost.h>
#include "KisCanvasCursorToken.h"
#include <pk/geometry/PkPoint.h>
#include <pk/geometry/PkPainterPath.h>
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
class QCursor;

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
    virtual KisCanvasCursorToken loadCursorResource(const PkString &resource,
                                                    const PkSize &size,
                                                    const PkPoint &hotspot) const override = 0;
    /**
     * Import a platform cursor shape (no resource, no hotspot) as a snapshot.
     *
     * This interface is the only host surface the retained tools have, and
     * KoToolBase::useCursor(Pk::CursorShape) is core API that the retained call
     * sites use unconditionally, so "produce a token for this shape" is a
     * question every host must be able to answer. Leaving it at the flake
     * default (token zero = platform default cursor) silently degrades those
     * call sites to the default arrow, which no test can distinguish from a
     * host that genuinely serves the shape. Making it pure here turns that into
     * a compile-time contract instead of a silent runtime downgrade.
     *
     * The implementation needs the platform cursor type, so only a Qt-bucket
     * host translation unit can provide it.
     */
    virtual KisCanvasCursorToken toolShapeCursorToken(Pk::CursorShape shape) const override = 0;
    /**
     * Canvas-side cursor capability. This is not a member of the flake host
     * contract any more: importing a platform cursor needs the platform cursor
     * type, which the retained (native) tools must not name. Only libs/canvas,
     * which is not part of the seam scan, keeps the operation.
     */
    virtual KisCanvasCursorToken toolImportCursor(const QCursor &cursor) const = 0;
    /** Snapshot oracle for the tokens this canvas produced. */
    virtual const QCursor *toolCursorSnapshot(KisCanvasCursorToken cursor) const = 0;
    /**
     * Ownership truth for this canvas. Zero is the platform-default cursor and
     * belongs to every host; a nonzero token belongs only to the host that can
     * resolve it. Delegating to toolCursorSnapshot() keeps that a single fact,
     * so a concrete canvas has to answer the snapshot question and nothing else.
     */
    virtual bool toolOwnsCursor(KisCanvasCursorToken cursor) const override
    {
        return cursor.value() == 0 || toolCursorSnapshot(cursor) != nullptr;
    }
    virtual KisCanvasCursorToken toolCursorToken(CursorStyle style) const = 0;
    virtual KisCanvasCursorToken toolMoveCursorToken() const = 0;
    virtual KisCanvasCursorToken toolMoveSelectionCursorToken() const = 0;
    virtual KisCanvasCursorToken toolSamplerCursorToken() const = 0;
    virtual KisCanvasCursorToken toolOpenHandCursorToken() const = 0;
    virtual KisCanvasCursorToken toolClosedHandCursorToken() const = 0;
    virtual KisCanvasCursorToken toolForbiddenCursorToken() const = 0;
    virtual KisCanvasCursorToken toolLoadCursorToken(const PkString &name,
                                                     int hotX,
                                                     int hotY) const = 0;
    virtual void toolApplyCursor(KisCanvasCursorToken cursor) override = 0;
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
    virtual void toolSetPriorityEventFilter(PkObject *filter, bool attached) = 0;
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
