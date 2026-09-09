/*
 * Canvas-local, toolkit-neutral declaration of the retained flake pointer
 * event ABI. The owning implementation remains in kritaflake.
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#ifndef KOPOINTEREVENT_H
#define KOPOINTEREVENT_H

#include <PkAuxTypes.h>
#include <PkNamespace.h>
#include <PkPoint.h>
#include <PkScopedPointer.h>

class KoPointerEvent
{
public:
    KoPointerEvent(const PkPoint &widgetPosition,
                   const PkPointF &documentPosition,
                   Pk::MouseButton button,
                   Pk::MouseButtons buttons,
                   Pk::KeyboardModifiers modifiers);
    KoPointerEvent(KoPointerEvent *event, const PkPointF &point);
    ~KoPointerEvent();
    KoPointerEvent(const KoPointerEvent &rhs);
    KoPointerEvent &operator=(const KoPointerEvent &rhs);

    KoPointerEvent detachedCopy() const;
    void accept();
    void ignore();
    Pk::KeyboardModifiers modifiers() const;
    bool isAccepted() const;
    bool spontaneous() const;
    Pk::MouseButton button() const;
    Pk::MouseButtons buttons() const;
    PkPoint globalPos() const;
    PkPoint pos() const;
    qreal pressure() const;
    qreal rotation() const;
    qreal tangentialPressure() const;
    int x() const;
    qreal xTilt() const;
    int y() const;
    qreal yTilt() const;
    int z() const;
    ulong time() const;
    bool isTabletEvent() const;
    bool isTouchEvent() const;
    static bool tabletInputReceived();

    PkPointF point;

private:
    class Private;
    const PkScopedPointer<Private> d;
};

#endif // KOPOINTEREVENT_H
