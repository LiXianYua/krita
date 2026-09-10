/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006, 2008 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2007-2010 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-FileCopyrightText: 2007-2008 C. Boemann <cbo@boemann.dk>
 * SPDX-FileCopyrightText: 2006-2007 Jan Hambrecht <jaham@gmx.net>
 * SPDX-FileCopyrightText: 2009 Thorsten Zachmann <zachmann@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KOCANVASCONTROLLER_H
#define KOCANVASCONTROLLER_H
#include <PkObject.h>
#include <PkRect.h>

#include "kritaflake_export.h"
#include "KoCanvasActionHost.h"
#include <QObject>

#include <PkSize.h>
#include <PkPoint.h>
#include <PkPoint.h>
#include <PkPointer.h>

#include <KoZoomState.h>

class PkRect;
class PkRectF;


class KoShape;
class KoCanvasBase;
class KoCanvasControllerProxyObject;
class KoViewTransformStillPoint;

/**
 * KoCanvasController is the base class for wrappers around your canvas
 * that provides scrolling and zooming for your canvas.
 *
 * Flake does not provide a canvas, the application will have to
 * implement a canvas themselves. You canvas can be QWidget-based
 * or something we haven't invented yet -- as long the class that holds the canvas
 * implements KoCanvasController, tools, scrolling and zooming will work.
 *
 * A KoCanvasController implementation acts as a decorator around the canvas widget
 * and provides a way to scroll the canvas, allows the canvas to be centered
 * in the viewArea and manages tool activation.
 *
 * <p>The using application can instantiate this class and add its
 * canvas using the setCanvas() call. Which is designed so it can be
 * called multiple times if you need to exchange one canvas
 * widget for another, for instance, switching between a plain QWidget or a QOpenGLWidget.
 *
 * <p>There is _one_ KoCanvasController per canvas in your
 * application.
 *
 * <p>The canvas widget is at most as big as the viewport of the scroll
 * area, and when the view on the document is near its edges, smaller.
 * In your canvas widget code, you can find the right place in your
 * document in view coordinates (pixels) by adding the documentOffset
 */
class KRITAFLAKE_EXPORT KoCanvasController : public KoCanvasActionHost
{
public:

    // proxy QObject: use this to connect to slots and signals.
    PkPointer<KoCanvasControllerProxyObject> proxyObject;

    /**
     * Constructor.
     * @param actionCollection the action collection for this canvas
     */
    explicit KoCanvasController(QObject *actionCollection);
    virtual ~KoCanvasController();

public:

    /**
     * Set the new canvas to be shown as a child
     * Calling this will canvasRemoved() if there was a canvas before, and will emit
     * canvasSet() with the new canvas.
     * @param canvas the new canvas. The KoCanvasBase::canvas() will be called to retrieve the
     *        actual widget which will then be added as child of this one.
     */
    virtual void setCanvas(KoCanvasBase *canvas) = 0;

    /**
     * Return the currently set canvas. The default implementation will return Null
     * @return the currently set canvas
     */
    virtual KoCanvasBase *canvas() const;

    /**
     * @brief Scrolls the content of the canvas so that the given rect is visible.
     *
     * The rect is to be specified in document coordinates (points). The scrollbar positions
     * are changed so that the centerpoint of the rectangle is centered if possible.
     *
     * @param rect the rectangle to make visible
     * @param smooth if true the viewport translation will make be just enough to ensure visibility, no more.
     */
    virtual void ensureVisibleDoc(const PkRectF &docRect, bool smooth) = 0;

    /**
     * @brief zooms in keeping @p stillPoint not moved.
     */
    virtual void zoomIn(const KoViewTransformStillPoint &stillPoint) = 0;
    virtual void zoomIn() = 0;

    /**
     * @brief zooms out keeping @p stillPoint not moved.
     */
    virtual void zoomOut(const KoViewTransformStillPoint &stillPoint) = 0;
    virtual void zoomOut() = 0;

    /**
     * @brief zoom so that rect is exactly visible (as close as possible)
     *
     * The rect must be specified in **widget** coordinates. The scrollbar positions
     * are changed so that the center of the rect becomes center if possible.
     *
     * @param rect the rect in **widget** coordinates that should fit the view afterwards
     */
    virtual void zoomTo(const PkRect &rect) = 0;

    virtual void setZoom(KoZoomMode::Mode mode, qreal zoom) = 0;

    /**
     * Sets the preferred center point in view coordinates (pixels).
     * @param viewPoint the new preferred center
     */
    virtual void setPreferredCenter(const PkPointF &viewPoint) = 0;

    /// Returns the currently set preferred center point in view coordinates (pixels)
    virtual PkPointF preferredCenter() const = 0;

    /**
     * Move the canvas over the x and y distance of the parameter distance
     * @param distance the distance in view coordinates (pixels).  A positive distance means moving the canvas up/left.
     */
    virtual void pan(const PkPoint &distance) = 0;

    /**
     * Move the canvas up. This behaves the same as \sa pan() with a positive y coordinate.
     */
    virtual void panUp() = 0;

    /**
     * Move the canvas down. This behaves the same as \sa pan() with a negative y coordinate.
     */
    virtual void panDown() = 0;

    /**
     * Move the canvas to the left. This behaves the same as \sa pan() with a positive x coordinate.
     */
    virtual void panLeft() = 0;

    /**
     * Move the canvas to the right. This behaves the same as \sa pan() with a negative x coordinate.
     */
    virtual void panRight() = 0;

    /**
     * Get the position of the scrollbar
     */
    virtual PkPoint scrollBarValue() const = 0;

    /**
     * Set the position of the scrollbar
     * @param value the new values of the scroll bars
     */
    virtual void setScrollBarValue(const PkPoint &value) = 0;

    /**
     * Update the range of scroll bars
     */
    virtual void resetScrollBars() = 0;

   /**
     * Returns the action collection for the window
     *
     * The returned QObject acts as an action repository. Actions are added
     * with setParent(actionCollection) and must have their objectName set to
     * the action name, because they are later retrieved via
     * findChild<QAction *>(name)/findChildren<QAction *>().
     *
     * @returns action collection for this window, can be 0
     */
    QObject *actionCollection() const;

    /**
     * 宿主动作集合的桶无关回报（KoCanvasActionHost）。
     *
     * 管理器（KoToolManager）不再认识 QAction：它拿到的是 identity + 已编码的
     * shortcut chord，逐条决定启用/禁用后再经 setHostActionEnabled() 写回。
     * 快捷键载荷的解码（丢空 chord、逐和弦取 int）就发生在这里，管理器只做等值比较。
     */
    PkList<KisHostActionIdentity> hostActions() const override;

    /** 按 objectName 写回启用态。找不到该名字时是 no-op。 */
    void setHostActionEnabled(const PkString &objectName, bool enabled) override;

    /**
     * @return the current position of the cursor fetched from QCursor::pos() and
     *         converted into document coordinates
     */
    virtual PkPointF currentCursorPosition() const = 0;

    virtual KoZoomState zoomState() const = 0;

protected:
    void setDocumentOffset(const PkPoint &offset);


private:
    class Private;
    Private * const d;
};


/**
 * Native notification and lifetime object for KoCanvasController. Keeping it
 * separate lets a controller use its own host inheritance independently.
 */
class KRITAFLAKE_EXPORT KoCanvasControllerProxyObject : public PkObject
{
public:
    explicit KoCanvasControllerProxyObject(KoCanvasController *canvasController, PkObject *parent = nullptr);

public:

    // Convenience methods to invoke the signals from subclasses

    void emitCanvasRemoved(KoCanvasController *canvasController) { canvasRemoved(canvasController); }
    void emitCanvasSet(KoCanvasController *canvasController) { canvasSet(canvasController); }
    void emitCanvasOffsetChanged() { canvasOffsetChanged(); }
    void emitCanvasMousePositionChanged(const PkPoint &position) { canvasMousePositionChanged(position); }
    void emitDocumentMousePositionChanged(const PkPointF &position) { documentMousePositionChanged(position); }
    void emitSizeChanged(const PkSize &size) { sizeChanged(size); }
    void emitMoveDocumentOffset(const PkPointF &oldOffset, const PkPointF &newOffset) { moveDocumentOffset(oldOffset, newOffset); }
    void emitEffectiveZoomChanged(qreal zoom) { effectiveZoomChanged(zoom); }
    void emitZoomStateChanged(const KoZoomState &zoomState) { zoomStateChanged(zoomState); }
    void emitDocumentRectInWidgetPixelsChanged(const PkRectF &documentRectInWidgetPixels) { documentRectInWidgetPixelsChanged(documentRectInWidgetPixels); }
    void emitDocumentRotationChanged(qreal angle) { documentRotationChanged(angle); }
    void emitDocumentMirrorStatusChanged(bool mirrorX, bool mirrorY) { documentMirrorStatusChanged(mirrorX, mirrorY); }
    void emitCanvasStateChanged() { canvasStateChanged(); }

    // Convenience method to retrieve the canvas controller for who needs to use PkPointer
    KoCanvasController *canvasController() const { return m_canvasController; }

public:
    /**
     * Emitted when a previously added canvas is about to be removed.
     * @param canvasController this object
     */
    void canvasRemoved(KoCanvasController *canvasController) { activateSignal(this, PkMemberFnKey::from(&KoCanvasControllerProxyObject::canvasRemoved), canvasController); }

    /**
     * Emitted when a canvas is set on this widget
     * @param canvasController this object
     */
    void canvasSet(KoCanvasController *canvasController) { activateSignal(this, PkMemberFnKey::from(&KoCanvasControllerProxyObject::canvasSet), canvasController); }

    /**
     * Emitted when canvasOffset() changes
     */
    void canvasOffsetChanged() { activateSignal(this, PkMemberFnKey::from(&KoCanvasControllerProxyObject::canvasOffsetChanged)); }

    /**
     * Emitted when the cursor is moved over the canvas widget.
     * @param position the position in view coordinates (pixels).
     */
    void canvasMousePositionChanged(const PkPoint &position) { activateSignal<const PkPoint &>(this, PkMemberFnKey::from(&KoCanvasControllerProxyObject::canvasMousePositionChanged), position); }

    /**
     * Emitted when the cursor is moved over the canvas widget.
     * @param position the position in document coordinates.
     *
     * Use \ref canvasMousePositionChanged to get the position
     * in view coordinates.
     */
    void documentMousePositionChanged(const PkPointF &position) { activateSignal<const PkPointF &>(this, PkMemberFnKey::from(&KoCanvasControllerProxyObject::documentMousePositionChanged), position); }

    /**
     * Emitted when the entire controller size changes
     * @param size the size in widget pixels.
     */
    void sizeChanged(const PkSize &size) { activateSignal<const PkSize &>(this, PkMemberFnKey::from(&KoCanvasControllerProxyObject::sizeChanged), size); }

    /**
     * Emitted whenever the document is scrolled.
     *
     * @param point the new top-left point from which the document should
     * be drawn.
     */
    void moveDocumentOffset(const PkPointF &oldOffset, const PkPointF &newOffset) { activateSignal<const PkPointF &, const PkPointF &>(this, PkMemberFnKey::from(&KoCanvasControllerProxyObject::moveDocumentOffset), oldOffset, newOffset); }

    void effectiveZoomChanged(qreal zoom) { activateSignal(this, PkMemberFnKey::from(&KoCanvasControllerProxyObject::effectiveZoomChanged), zoom); }

    void zoomStateChanged(const KoZoomState &zoomState) { activateSignal<const KoZoomState &>(this, PkMemberFnKey::from(&KoCanvasControllerProxyObject::zoomStateChanged), zoomState); }

    void documentRectInWidgetPixelsChanged(const PkRectF &rect) { activateSignal<const PkRectF &>(this, PkMemberFnKey::from(&KoCanvasControllerProxyObject::documentRectInWidgetPixelsChanged), rect); }

    void documentRotationChanged(qreal angle) { activateSignal(this, PkMemberFnKey::from(&KoCanvasControllerProxyObject::documentRotationChanged), angle); }
    void documentMirrorStatusChanged(bool mirrorX, bool mirrorY) { activateSignal(this, PkMemberFnKey::from(&KoCanvasControllerProxyObject::documentMirrorStatusChanged), mirrorX, mirrorY); }

    void canvasStateChanged() { activateSignal(this, PkMemberFnKey::from(&KoCanvasControllerProxyObject::canvasStateChanged)); }

private:
    KoCanvasController *m_canvasController;
};

#endif
