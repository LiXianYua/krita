/* This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2006, 2010 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-FileCopyrightText: 2006-2010 Thomas Zander <zander@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#ifndef _KO_TOOL_PROXY_H_
#define _KO_TOOL_PROXY_H_

#include "kritaflake_export.h"

#include <QObject>
#include <PkObject.h>
// [migrate] missing include for Pk/Qt type
#include <PkString.h>
// [migrate] missing include for Pk/Qt type
#include <PkVariant.h>
// [migrate] missing include for Pk/Qt type
#include <PkVector.h>
#include <PkNamespace.h>

class QAction;
class QKeyEvent;
class QWheelEvent;
// Upstream relied on the real `<QObject>` to declare QEvent transitively. The
// compat routing (pk/signal/compat/QObject) does not, and `processEvent()` only
// ever takes a pointer, so a forward declaration is all the native compile
// interface needs. Same form as `libs/flake/KoPointerEvent.h`.
class QEvent;
class KoCanvasBase;
class KoViewConverter;
class KoToolBase;
class KoToolProxyPrivate;
class QInputMethodEvent;
class KoPointerEvent;
class KoInputDevice;
class PkTabletEvent;
class PkInputEvent;
class PkTouchEvent;
class QDragMoveEvent;
class QDragLeaveEvent;
class QDropEvent;
class QFocusEvent;
class PkPainter;
class PkPointF;
class QMenu;
class KisPopupWidgetInterface;

enum class KoPointerInputSource {
    None,
    Mouse,
    Tablet,
    Touch
};

/**
 * Tool proxy object which allows an application to address the current tool.
 *
 * Applications typically have a canvas and a canvas requires a tool for
 * the user to do anything.  Since the flake system is responsible for handling
 * tools and also to change the active tool when needed we provide one class
 * that can be used by an application canvas to route all the native events too
 * which will transparently be routed to the active tool.  Without the application
 * having to bother about which tool is active.
 */
// The proxy carries two object identities at once: the host toolkit object
// surface (QObject: object tree / QPointer / event loop, retained by the
// 2026-09-05 ruling and handed to M5) and the native PkObject signal/lifetime
// surface. Under the compat routing (pk/signal/compat/QObject) the token
// `QObject` *is* `PkObject`, so naming both bases duplicates the base class;
// the base list is therefore spelled per configuration.
#if defined(QT_CORE_LIB)
class KRITAFLAKE_EXPORT KoToolProxy : public QObject, public PkObject
#else
class KRITAFLAKE_EXPORT KoToolProxy : public PkObject
#endif
{
public:
    /**
     * Constructor
     * @param canvas Each canvas has 1 toolProxy. Pass the parent here.
     * @param parent a parent QObject for memory management purposes.
     */
    explicit KoToolProxy(KoCanvasBase *canvas, QObject *parent = 0);
    ~KoToolProxy() override;

    /// Canonical decoration dispatch. The host supplies a native PkPainter backend.
    void paint(PkPainter &painter, const KoViewConverter &converter);

    /// Forwarded to the current KoToolBase
    void repaintDecorations();

    /// Forwarded to the current KoToolBase
    void tabletEvent(const KoInputDevice &id, const PkTabletEvent &event, const PkPointF &point);

    /// Forwarded to the current KoToolBase
    void mousePressEvent(const PkInputEvent &event, const PkPointF &point);
    void mousePressEvent(KoPointerEvent *event);

    /// Forwarded to the current KoToolBase
    void mouseDoubleClickEvent(const PkInputEvent &event, const PkPointF &point);
    void mouseDoubleClickEvent(KoPointerEvent *event);

    /// Forwarded to the current KoToolBase
    void mouseMoveEvent(const PkInputEvent &event, const PkPointF &point);
    void mouseMoveEvent(KoPointerEvent *event);

    /// Forwarded to the current KoToolBase
    void mouseReleaseEvent(const PkInputEvent &event, const PkPointF &point);
    void mouseReleaseEvent(KoPointerEvent *event);

    /// Forwarded to the current KoToolBase
    void keyPressEvent(QKeyEvent *event);

    /// Forwarded to the current KoToolBase
    void keyReleaseEvent(QKeyEvent *event);

    /// Forwarded to the current KoToolBase
    void explicitUserStrokeEndRequest();

    /// Forwarded to the current KoToolBase
    PkVariant inputMethodQuery(Pk::InputMethodQuery query) const;

    /// Forwarded to the current KoToolBase
    void inputMethodEvent(QInputMethodEvent *event);

    /// Forwarded to the current KoToolBase
    void focusInEvent(QFocusEvent *event);

    /// Forwarded to the current KoToolBase
    void focusOutEvent(QFocusEvent *event);

    /// Forwarded to the current KoToolBase
    QMenu* popupActionsMenu();

    /// Forwarded to the current KoToolBase
    KisPopupWidgetInterface* popupWidget();

    /// Forwarded to the current KoToolBase
    void deleteSelection();

    /// This method gives the proxy a chance to do things. for example it is need to have working singlekey
    /// shortcuts. call it from the canvas' event function and forward it to QWidget::event() later.
    void processEvent(QEvent *) const;

    /// returns true if the current tool holds a selection
    bool hasSelection() const;

    /// Forwarded to the current KoToolBase
    void cut();

    /// Forwarded to the current KoToolBase
    void copy() const;

    /// Forwarded to the current KoToolBase
    bool paste();

    /// Forwarded to the current KoToolBase
    bool selectAll();

    /// Forwarded to the current KoToolBase
    void deselect();

    /// Forwarded to the current KoToolBase
    void dragMoveEvent(QDragMoveEvent *event, const PkPointF &point);

    /// Forwarded to the current KoToolBase
    void dragLeaveEvent(QDragLeaveEvent *event);

    /// Forwarded to the current KoToolBase
    void dropEvent(QDropEvent *event, const PkPointF &point);

    /// Set the new active tool.
    virtual void setActiveTool(KoToolBase *tool);

    void touchEvent(const PkTouchEvent &event, const PkPointF& point);

    KoPointerEvent* lastDeliveredPointerEvent() const;

    /// \internal
    KoToolProxyPrivate *priv();

protected:
    /// Forwarded to the current KoToolBase
    void requestUndoDuringStroke();

    /// Forwarded to the current KoToolBase
    void requestRedoDuringStroke();

    /// Forwarded to the current KoToolBase
    void requestStrokeCancellation();

    /// Forwarded to the current KoToolBase
    void requestStrokeEnd();

public:
    /**
     * A tool can have a selection that is copy-able, this signal is emitted when that status changes.
     * @param hasSelection is true when the tool holds selected data.
     */
    void selectionChanged(bool hasSelection);

    /**
     * Emitted every time a tool is changed.
     * @param toolId the id of the tool.
     * @see KoToolBase::toolId()
     */
    void toolChanged(const PkString &toolId);

protected:
    virtual PkPointF widgetToDocument(const PkPointF &widgetPoint) const = 0;
    virtual PkPointF documentToWidget(const PkPointF &documentPoint) const = 0;
    KoCanvasBase* canvas() const;
    int multiClickCount() const;

private:
    void countMultiClick(KoPointerEvent *ev, KoPointerInputSource source);

    friend class KoToolProxyPrivate;
    KoToolProxyPrivate * const d;
};

#endif // _KO_TOOL_PROXY_H_
