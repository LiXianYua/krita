/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2011 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#ifndef KOTOOLBASE_H
#define KOTOOLBASE_H

#include <pk/signal/PkObject.h>
#include <pk/render/PkPainter.h>
#include <PkSharedPointer.h>
#include <PkSet.h>
#include <PkList.h>
#include <PkHash.h>
#include <PkVariant.h>
#include <PkNamespace.h>
#include "KoCanvasCursorHost.h"

#include <PkStringList.h>
#include "kritaflake_export.h"
#include <functional>

class KoShape;
class KoCanvasBase;
class KoPointerEvent;
class KoViewConverter;
class KoToolFactoryBase;
class KoToolSelection;
class KoToolBasePrivate;
class KoShapeControllerBase;
class KisPopupWidgetInterface;
class KoDerivedResourceConverter;
class KoAbstractCanvasResourceInterface;

typedef PkSharedPointer<KoDerivedResourceConverter> KoDerivedResourceConverterSP;
typedef PkSharedPointer<KoAbstractCanvasResourceInterface> KoAbstractCanvasResourceInterfaceSP;

class QKeyEvent;
class PkString;
class PkRectF;
class PkPointF;
class QTouchEvent;
class QMenu;

struct KRITAFLAKE_EXPORT PkToolPointerEventData
{
    PkPointF point;
    qreal pressure = 1.0;
    qreal rotation = 0.0;
    qreal xTilt = 0.0;
    qreal yTilt = 0.0;
    int widgetX = 0;
};

struct KRITAFLAKE_EXPORT PkToolSelectedShapes
{
    KoShape *first = nullptr;
    int count = 0;
};

/** Toolkit-neutral key payload dispatched by the host-facing key adapter. */
class KRITAFLAKE_EXPORT PkToolKeyEvent
{
public:
    PkToolKeyEvent(Pk::Key key, Pk::KeyboardModifiers modifiers, bool accepted,
                   bool autoRepeat = false, const PkString &text = PkString())
        : m_key(key)
        , m_modifiers(modifiers)
        , m_accepted(accepted)
        , m_autoRepeat(autoRepeat)
        , m_text(text)
    {
    }

    Pk::Key key() const { return m_key; }
    Pk::KeyboardModifiers modifiers() const { return m_modifiers; }
    void accept() { m_accepted = true; }
    void ignore() { m_accepted = false; }
    bool isAccepted() const { return m_accepted; }
    bool isAutoRepeat() const { return m_autoRepeat; }
    const PkString &text() const { return m_text; }

private:
    Pk::Key m_key;
    Pk::KeyboardModifiers m_modifiers;
    bool m_accepted;
    bool m_autoRepeat;
    PkString m_text;
};

class KRITAFLAKE_EXPORT PkToolEvent
{
public:
    explicit PkToolEvent(bool accepted = false) : m_accepted(accepted) {}
    void accept() { m_accepted = true; }
    void ignore() { m_accepted = false; }
    bool isAccepted() const { return m_accepted; }

private:
    bool m_accepted;
};

enum class PkToolInputMethodAttributeType { Selection, Cursor, TextFormat };
enum class PkToolInputMethodLineStyle { Solid, Dotted, Dashed, Wavy };

struct KRITAFLAKE_EXPORT PkToolInputMethodTextFormat {
    bool underline = false;
    bool overline = false;
    bool strikeOut = false;
    bool thick = false;
    PkToolInputMethodLineStyle style = PkToolInputMethodLineStyle::Solid;
};

struct KRITAFLAKE_EXPORT PkToolInputMethodAttribute {
    PkToolInputMethodAttributeType type = PkToolInputMethodAttributeType::Cursor;
    int start = 0;
    int length = 0;
    PkToolInputMethodTextFormat format;
};

class KRITAFLAKE_EXPORT PkToolInputMethodEvent : public PkToolEvent
{
public:
    PkString commitString;
    PkString preeditString;
    int replacementStart = 0;
    int replacementLength = 0;
    PkList<PkToolInputMethodAttribute> attributes;
};

/**
 * Abstract base class for all tools. Tools can create or manipulate
 * flake shapes, canvas state or any other thing that a user may wish
 * to do to his document or his view on a document with a pointing
 * device.
 *
 * There exists an instance of every tool for every pointer device.
 * These instances are managed by the toolmanager..
 */
class KRITAFLAKE_EXPORT KoToolBase : public PkObject
{
public:
    /**
     * Constructor, normally only called by the factory (see KoToolFactoryBase)
     * @param canvas the canvas interface this tool will work for.
     */
    explicit KoToolBase(KoCanvasBase *canvas);
    ~KoToolBase() override;

    /**
     * The factory holds properties common to all instances of the same
     * tool type, such as the string identifying the class or the icon name.
     *
     * Not all tool instances have a factory reference, for example the
     * "delegated" inner implementations to which events are forwarded
     * don't have one, as well as instances created by regression tests.
     *
     * Every instance used by the tool manager has a factory reference and
     * a tool identifier, but they're not available during construction.
     */
    KoToolFactoryBase* factory() const;

    virtual PkRectF decorationsRect() const;

    /**
     * Return if dragging (moving with the mouse down) to the edge of a canvas should scroll the
     * canvas (default is true).
     * @return if this tool wants mouse events to cause scrolling of canvas.
     */
    virtual bool wantsAutoScroll() const;

    /**
     * Called by the canvas to paint any decorations that the tool deems needed.
     * The painter has the top left of the canvas as its origin.
     * @param painter used for painting the shape
     * @param converter to convert between internal and view coordinates.
     */
    // The host supplies a Pk backend through KoToolProxy; this is the only decoration API.
    virtual void paint(PkPainter &painter, const KoViewConverter &converter) { Q_UNUSED(painter); Q_UNUSED(converter); }

    /**
     * Called when (one of) the mouse or stylus buttons is pressed.
     * Implementors should call event->ignore() if they do not actually use the event.
     * @param event state and reason of this mouse or stylus press
     */
    virtual void mousePressEvent(KoPointerEvent *event) = 0;

    /**
     * Called when (one of) the mouse or stylus buttons is double clicked.
     * Implementors should call event->ignore() if they do not actually use the event.
     * Default implementation ignores this event.
     * @param event state and reason of this mouse or stylus press
     */
    virtual void mouseDoubleClickEvent(KoPointerEvent *event);

    /**
     * Called when (one of) the mouse or stylus buttons is triple clicked.
     * Implementors should call event->ignore() if they do not actually use the event.
     * Default implementation ignores this event.
     * @param event state and reason of this mouse or stylus press
     */
    virtual void mouseTripleClickEvent(KoPointerEvent *event);

    /**
     * Called when the mouse or stylus moved over the canvas.
     * Implementors should call event->ignore() if they do not actually use the event.
     * @param event state and reason of this mouse or stylus move
     */
    virtual void mouseMoveEvent(KoPointerEvent *event) = 0;

    /**
     * Called when (one of) the mouse or stylus buttons is released.
     * Implementors should call event->ignore() if they do not actually use the event.
     * @param event state and reason of this mouse or stylus release
     */
    virtual void mouseReleaseEvent(KoPointerEvent *event) = 0;

    /**
     * Called when a key is pressed.
     * Implementors should call event->ignore() if they do not actually use the event.
     * Default implementation ignores this event.
     * @param event state and reason of this key press
     */
    virtual void pkKeyPressEvent(PkToolKeyEvent *event);

    /**
     * Called when a key is released
     * Implementors should call event->ignore() if they do not actually use the event.
     * Default implementation ignores this event.
     * @param event state and reason of this key release
     */
    virtual void pkKeyReleaseEvent(PkToolKeyEvent *event);

    /**
     * @brief explicitUserStrokeEndRequest is called by the input manager
     *        when the user presses Enter key or any equivalent. This callback
     *        comes before requestStrokeEnd(), which comes from a different source.
     */
    virtual void explicitUserStrokeEndRequest();

    /**
     * This method is used to query a set of properties of the tool to be
     * able to support complex input method operations as support for surrounding
     * text and reconversions.
     * Default implementation returns simple defaults, for tools that want to provide
     * a more responsive text entry experience for CJK languages it would be good to reimplement.
     * @param query specifies which property is queried.
     * @param converter the view converter for the current canvas.
     */
    virtual PkVariant inputMethodQuery(Pk::InputMethodQuery query) const;

    /**
     * Text entry of complex text, like CJK, can be made more interactive if a tool
     * implements this and the InputMethodQuery() methods.
     * Reimplementing this only provides the user with a more responsive text experience, since the
     * default implementation forwards the typed text as key pressed events.
     * @param event the input method event.
     */
    virtual void inputMethodEvent(PkToolInputMethodEvent *event);

    /**
     * This passes on the focusInEven from the canvas widget, which can be used to activate
     * animating decorations (like a cursor blink effect in the text tool).
     */
    virtual void focusInEvent(PkToolEvent *event);

    /**
     * This passes on the focusInEven from the canvas widget, which can be used to deactivate
     * animating decorations (like a cursor blink effect in the text tool).
     */
    virtual void focusOutEvent(PkToolEvent *event);

    /**
     * Called when (one of) a custom device buttons is pressed.
     * Implementors should call event->ignore() if they do not actually use the event.
     * @param event state and reason of this custom device press
     */
    virtual void customPressEvent(KoPointerEvent *event);

    /**
     * Called when (one of) a custom device buttons is released.
     * Implementors should call event->ignore() if they do not actually use the event.
     * @param event state and reason of this custom device release
     */
    virtual void customReleaseEvent(KoPointerEvent *event);

    /**
     * Called when a custom device moved over the canvas.
     * Implementors should call event->ignore() if they do not actually use the event.
     * @param event state and reason of this custom device move
     */
    virtual void customMoveEvent(KoPointerEvent *event);

    /**
     * @return true if synthetic mouse events on the canvas should be eaten.
     *
     * For example, the guides tool should allow click and drag with touch,
     * while the same touch events should be rejected by the freehand tool.
     *
     * These events are sent by the OS in Windows
     */
    bool maskSyntheticEvents() const;

    /**
     * @return true if the tool uses preset opacity, false if it uses tool.
     */
    bool isOpacityPresetMode() const;

    /**
     * get the identifier code from the KoToolFactoryBase that created this tool.
     * @return the toolId.
     * @see KoToolFactoryBase::id()
     */
    PkString toolId() const;

    /// Return the host-scoped native cursor token last emitted by this tool.
    KisCanvasCursorToken cursorToken() const;

    /**
     * Returns the internal selection object of this tool.
     * Each tool can have a selection which is private to that tool and the specified shape that it comes with.
     * The default returns 0.
     */
    virtual KoToolSelection *selection();

    /**
     * Bucket-agnostic forward for hasSelection(): KoToolSelection is only defined
     * in the Qt translation unit, and this definition lives there too, so the
     * selection object is dereferenced with the real (Qt) layout.
     */
    bool selectionHasSelection();

    /**
     * @returns true if the tool has selected data.
     */
    virtual bool hasSelection();

    /**
     * copies the tools selection to the clipboard.
     * The default implementation is empty to aid tools that don't have any selection.
     * @see selection()
     */
    virtual void copy() const;

    /**
     * Delete the tools selection.
     * The default implementation is empty to aid tools that don't have any selection.
     * @see selection()
     */
    virtual void deleteSelection();

    /**
     * Cut the tools selection and copy it to the clipboard.
     * The default implementation calls copy() and then deleteSelection()
     * @see copy()
     * @see deleteSelection()
     */
    virtual void cut();

    /**
     * Paste the clipboard selection.
     * A tool typically has one or more shapes selected and pasting should do something meaningful
     * for this specific shape and tool combination.  Inserting text in a text tool, for example.
     * @return will return true if pasting succeeded. False if nothing happened.
     */
    virtual bool paste();

    /**
     * @brief selectAll
     * select all data the tool can select.
     * @return true if something happened, false if nothing happened.
     */
    virtual bool selectAll();

    /**
     * @brief deselect
     * the tool should clear the selection if it has one.
     */
    virtual void deselect();

    /**
     * Handle the dragMoveEvent
     * A tool typically has one or more shapes selected and dropping into should do
     * something meaningful for this specific shape and tool combination. For example
     * dropping text in a text tool.
     * The tool should Accept the event if it is meaningful; Default implementation does not.
     */
    virtual void dragMoveEvent(PkToolEvent *event, const PkPointF &point);

    /**
     * Handle the dragLeaveEvent
     * Basically just a notification that the drag is no long relevant
     * The tool should Accept the event if it is meaningful; Default implementation does not.
     */
    virtual void dragLeaveEvent(PkToolEvent *event);

    /**
     * Handle the dropEvent
     * A tool typically has one or more shapes selected and dropping into should do
     * something meaningful for this specific shape and tool combination. For example
     * dropping text in a text tool.
     * The tool should Accept the event if it is meaningful; Default implementation does not.
     */
    virtual void dropEvent(PkToolEvent *event, const PkPointF &point);

    /**
     * @return a menu with context-aware actions for the current selection. If
     *         the returned value is null, no context menu is shown.
     */
    virtual QMenu* popupActionsMenu() {return nullptr;}

    /**
     * @return a widget with useful controls to be popped up on top of the canvas.
     *         Will not be called if `popupActionsMenu()` does not return null.
     */
    virtual KisPopupWidgetInterface* popupWidget() {return nullptr;}

    /// Returns the canvas the tool is working on
    KoCanvasBase *canvas() const;

    /**
      * This method can be reimplemented in a subclass.
      * @return returns true, if the tool is in text mode. that means, that there is
      *   any kind of textual input and all single key shortcuts should be eaten.
      */
    bool isInTextMode() const;

    /**
     * @brief decorationThickness
     * The minimum thickness for tool decoration lines,
     * this is derived from the screen magnification, thus the HiDPI settings.
     * Note: to use this effectively, also set the pen to isCosmetic(true);
     * @return the minimum thickness for decoration lines in pixels.
     */
    int decorationThickness() const;

public:

    /**
     * Called when the user requested undo while the stroke is
     * active. If you tool supports undo of the part of its actions,
     * override this method and do the needed work there.
     *
     * NOTE: Default implementation forwards this request to
     *       requestStrokeCancellation() method, so that the stroke
     *       would be cancelled.
     */
    virtual void requestUndoDuringStroke();

    /**
     * Called when the user requested redo while the stroke is active. If your
     * tool supports redo and maintains an intermediate state which can
     * interfere with redo override this method to handle the state.
     */
    virtual void requestRedoDuringStroke();

    /**
     * Called when the user requested the cancellation of the current
     * stroke. If you tool supports cancelling, override this method
     * and do the needed work there
     */
    virtual void requestStrokeCancellation();

    /**
     * Called when the image decided that the stroke should better be
     * ended. If you tool supports long strokes, override this method
     * and do the needed work there
     */
    virtual void requestStrokeEnd();


    /**
     * This method is called when this tool instance is activated.
     * For any main window there is only one tool active at a time, which then gets all
     * user input.  Switching between tools will call deactivate on one and activate on the
     * new tool allowing the tool to flush items (like a selection)
     * when it is not in use.
     *
     * @param shapes the set of shapes that are selected or suggested for editing by a
     *      selected shape for the tool to work on.  Not all shapes will be meant for this
     *      tool.
     * @see deactivate()
     */
    virtual void activate(const PkSet<KoShape*> &shapes);

    /**
     * This method is called whenever this tool is no longer the
     * active tool
     * @see activate()
     */
    virtual void deactivate();

    /**
     * This method is called whenever a property in the resource
     * provider associated with the canvas this tool belongs to
     * changes. An example is currently selected foreground color.
     */
    virtual void canvasResourceChanged(int key, const PkVariant &res);

    /**
     * This method is called whenever a property in the resource
     * provider associated with the document this tool belongs to
     * changes. An example is the handle radius
     */
    virtual void documentResourceChanged(int key, const PkVariant &res);

    /**
     * This method just relays the given text via the tools statusTextChanged signal.
     * @param statusText the new status text
     */
    void setStatusText(const PkString &statusText);

    /**
     * request a repaint of the decorations to be made. This triggers
     * an update call on the canvas, but does not paint directly.
     */
    virtual void repaintDecorations();

    /**
     * force the update of the icons on the cached options widget
     */
    void updateOptionsWidgetIcons();

public:

    /**
     * Emitted when this tool wants itself to be replaced by another tool.
     *
     * @param id the identification of the desired tool
     * @see toolId(), KoToolFactoryBase::id()
     */
    void activateTool(const PkString &id);

    /**
     * Emitted by useCursor() when the cursor to display on the canvas is changed.
     * The KoToolManager should connect to this signal to handle cursors further.
     */
    void cursorTokenChanged(KisCanvasCursorToken cursor);

    /**
     * A tool can have a selection that is copy-able, this signal is emitted when that status changes.
     * @param hasSelection is true when the tool holds selected data.
     */
    void selectionChanged(bool hasSelection);

    /**
     * Emitted when the tool wants to display a different status text
     * @param statusText the new status text
     */
    void statusTextChanged(const PkString &statusText);

    /**
     * Emitted when the tool's text mode has changed.
     * @param inTextMode whether it is now in text mode.
     */
    void textModeChanged(bool inTextMode);

protected:
    void watchSelectedShapesChanged(std::function<void()> callback);
    PkToolPointerEventData pointerEventData(const KoPointerEvent *event) const;
    PkRectF documentRectToView(const KoViewConverter &converter, const PkRectF &rect) const;
    PkTransform documentToViewTransform(const KoViewConverter &converter) const;
    PkColor canvasForegroundColor() const;
    void requestCanvasUpdate(const PkRectF &rect);
    bool selectShapeAt(const PkPointF &point);
    bool addShapeToCanvas(KoShape *shape);
    PkToolSelectedShapes selectedShapes() const;

    /**
     * Apply a host-scoped token and publish the resulting cursor state.
     * Returns false for a foreign, stale, hostless, or otherwise rejected
     * token. Rejection leaves retained state unchanged, does not call the host
     * apply operation, and emits no cursor notification.
     */
    bool useCursor(KisCanvasCursorToken cursor);
    void useCursor(Pk::CursorShape cursorShape);

    /// Convenience function to get the current handle radius
    int handleRadius() const;

    /// Convenience function to get the current handle radius measured in document
    /// coordinates (points)
    qreal handleDocRadius() const;


    /// Convenience function to get the current grab sensitivity
    int grabSensitivity() const;

    /**
    * Returns a handle grab rect at the given position.
    *
    * The position is expected to be in document coordinates. The grab sensitivity
    * canvas resource is used for the dimension of the rectangle.
    *
    * @return the handle rectangle in document coordinates
    */
    PkRectF handleGrabRect(const PkPointF &position) const;

    /**
    * Returns a handle paint rect at the given position.
    *
    * The position is expected to be in document coordinates. The handle radius
    * canvas resource is used for the dimension of the rectangle.
    *
    * @return the handle rectangle in document coordinates
    */
    PkRectF handlePaintRect(const PkPointF &position) const;

    /**
      * You should set the text mode to true in subclasses, if this tool is in text input mode, eg if the users
      * are able to type. If you don't set it, then single key shortcuts will get the key event and not this tool.
      */
    void setTextMode(bool value);

    /**
     * Allows subclasses to specify whether synthetic mouse events should be accepted.
     */
    void setMaskSyntheticEvents(bool value);

    /**
     * Sets the opacity mode to either preset (true) or tool (false).
     */
    void setIsOpacityPresetMode(bool value);

    void setConverter(KoDerivedResourceConverterSP converter);

    void setAbstractResource(KoAbstractCanvasResourceInterfaceSP abstractResource);

    PkHash<int, KoAbstractCanvasResourceInterfaceSP> toolAbstractResources();
    PkHash<int, KoDerivedResourceConverterSP> toolConverters();

    /**
     * Returns true if activate() has been called (more times than deactivate :) )
     */
    bool isActivated() const;

    /**
     * Returns the last pointer event that was delivered to the canvas, the tool
     * belongs to. This event might be used as an approximation for the event0
     * handlers not having their own events, like activate()/deactivate().
     */
    KoPointerEvent* lastDeliveredPointerEvent() const;

protected:
    KoToolBase(KoToolBasePrivate &dd);

    KoToolBasePrivate *d_ptr;


private:

    friend class KoToolManager;

    /**
     * Set the KoToolFactoryBase that created this tool.
     * @param factory the KoToolFactoryBase
     * @see KoToolFactoryBase
     */
    void setFactory(KoToolFactoryBase *factory);

    KoToolBase();
    KoToolBase(const KoToolBase&);
    KoToolBase& operator=(const KoToolBase&);

    PK_DECLARE_PRIVATE(KoToolBase)
};

#endif /* KOTOOLBASE_H */

#include <pk/render/PkPainter.h>
