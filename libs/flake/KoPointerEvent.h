/* This file is part of the KDE project

   SPDX-FileCopyrightText: 2006 Thorsten Zachmann <zachmann@kde.org>
   SPDX-FileCopyrightText: 2006 C. Boemann Rasmussen <cbo@boemann.dk>
   SPDX-FileCopyrightText: 2006-2007 Thomas Zander <zander@kde.org>
   SPDX-FileCopyrightText: 2012 Boudewijn Rempt <boud@valdyas.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KOPOINTEREVENT_H

#define KOPOINTEREVENT_H

#include <PkAuxTypes.h>
#include <PkSharedPointer.h>
#include <PkPoint.h>
#include <PkPainterPath.h>
#include <PkPen.h>
#include <PkNamespace.h>
#include <optional>
#include <cstdint>

class QEvent;
class PkTabletEvent;
class PkInputEvent;
class PkTouchEvent;
// Only the mouse and touch carriers translate to Pk here; `QEvent` and
// `QWheelEvent` keep their Qt forward declarations for the steps that own
// those faces.
class QWheelEvent;

#include "kritaflake_export.h"
// [migrate] missing include for Pk/Qt type
#include <PkScopedPointer.h>

/**
 * KoPointerEvent is a synthetic event that can be built from a mouse,
 * touch or tablet event. In addition to always providing tools with tablet
 * pressure characteristics, KoPointerEvent has both the original
 * (canvas based) position as well as the normalized position, that is,
 * the position of the event _in_ the document coordinates.
 */
class KRITAFLAKE_EXPORT KoPointerEvent
{
public:
    /**
     * Constructor.
     *
     * @param event the host-carried mouse input event that is the base of this event.
     * @param point the zoomed point in the normal coordinate system.
     */
    KoPointerEvent(const PkInputEvent &event, const PkPointF &point);

    /**
     * Constructor.
     *
     * @param event the native tablet event that is the base of this event.
     * @param point the zoomed point in the normal coordinate system.
     */
    KoPointerEvent(const PkTabletEvent &event, const PkPointF &point);

    /**
     * Constructor.
     *
     * @param event the host-carried touch input event that is the base of this event.
     * @param point the zoomed point in the normal coordinate system.
     */
    KoPointerEvent(const PkTouchEvent &event, const PkPointF &point);

    /** Construct a host-free mouse event for retained tool dispatch. */
    KoPointerEvent(const PkPoint &widgetPosition,
                   const PkPointF &documentPosition,
                   Pk::MouseButton button,
                   Pk::MouseButtons buttons,
                   Pk::KeyboardModifiers modifiers);

    KoPointerEvent(KoPointerEvent *event, const PkPointF& point);

    ~KoPointerEvent();

    /**
     * Copies the event object.
     *
     * The observable input state is held by value, so the copy does not
     * depend on the lifetime of the event it was built from.
     */
    KoPointerEvent(const KoPointerEvent &rhs);

    /**
     * Copies the event object.
     *
     * See a comment in copy constructor for what is copied.
     */
    KoPointerEvent& operator=(const KoPointerEvent &rhs);

    /** Copy the observable input state. */
    KoPointerEvent detachedCopy() const;

    /**
     * For classes that are handed this event, you can choose to accept (default) this event.
     * Acceptance signifies that you have handled this event and found it useful, the effect
     * of that will be that the event will not be handled to other event handlers.
     */
    void accept();

    /**
     * For classes that are handed this event, you can choose to ignore this event.
     * Ignoring this event means you have not handled it and want to allow other event
     * handlers to try to handle it.
     */
    void ignore();

    /**
     * Returns the keyboard modifier flags that existed immediately before the event occurred.
     * See also QApplication::keyboardModifiers().
     */
    Pk::KeyboardModifiers modifiers() const;

    /// return if the event has been accepted.
    bool isAccepted() const;

    /// return if this event was spontaneous (see QMouseEvent::spontaneous())
    bool spontaneous() const;

    /// return button pressed (see QMouseEvent::button());
    Pk::MouseButton button() const;

    /// return buttons pressed (see QMouseEvent::buttons());
    Pk::MouseButtons buttons() const;

    /// Return the position screen coordinates
    PkPoint globalPos() const;

    /// return the position in widget coordinates
    PkPoint pos() const;

    /**
     * return the pressure (or a default value). The range is 0.0 - 1.0
     * and the default pressure (this is the pressure that will be given
     * when you use something like the mouse) is 1.0
     */
    double pressure() const;

    /// return the rotation (or a default value)
    double rotation() const;

    /**
     * return the tangential pressure  (or a default value)
     * This is typically given by a finger wheel on an airbrush tool. The range
     * is from -1.0 to 1.0. 0.0 indicates a neutral position. Current airbrushes can
     * only move in the positive direction from the neutral position. If the device
     * does not support tangential pressure, this value is always 0.0.
     */
    double tangentialPressure() const;

    /**
     * Return the x position in widget coordinates.
     * @see point
     */
    int x() const;

    /**
     * Returns the angle between the device (a pen, for example) and the
     * perpendicular in the direction of the x axis. Positive values are
     * towards the tablet's physical right. The angle is in the range -60
     * to +60 degrees. The default value is 0.
     */
    double xTilt() const;

    /**
     * Return the y position in widget coordinates.
     * @see point
     */
    int y() const;

    /**
     * Returns the angle between the device (a pen, for example) and the
     * perpendicular in the direction of the x axis. Positive values are
     * towards the tablet's physical right. The angle is in the range -60
     * to +60 degrees. The default value is 0.
     */
    double yTilt() const;

    /**
     * Returns the z position of the device. Typically this is represented
     * by a wheel on a 4D Mouse. If the device does not support a Z-axis,
     * this value is always zero. This is <em>not</em> the same as pressure.
     */
    int z() const;

    /**
     * Returns the time the event was registered.
     */
    std::uint64_t time() const;


    /// The point in document coordinates.
    PkPointF point;

    /**
     * Returns if the event comes from a tablet
     */
    bool isTabletEvent() const;

    /**
     * Returns if the event comes from a touch
     */
    bool isTouchEvent() const;

    /**
     * Whether we ever had any tablet inputs this session
     */
    static bool tabletInputReceived();

protected:
    friend class KoToolProxy;
    friend class KisToolProxy;
    friend class KisScratchPadEventFilter;
private:

    class Private;
    const PkScopedPointer<Private> d;
};

#endif
