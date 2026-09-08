/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2012 Arjen Hiemstra <ahiemstra@heimr.nl>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_abstract_input_action.h"

class KisAbstractInputAction::Private
{
public:
    PkString id;
    PkString name;
    PkString description;
    PkHash<PkString, int> indexes;

    PkPointF lastCursorPosition;
    PkPointF startCursorPosition;

    static KisInputManager *inputManager;
    static std::function<PkPointF(KisInputManager *, const PkNativeGestureEvent *)> nativeGestureMapper;
};

KisInputManager *KisAbstractInputAction::Private::inputManager = 0;
std::function<PkPointF(KisInputManager *, const PkNativeGestureEvent *)> KisAbstractInputAction::Private::nativeGestureMapper;

KisAbstractInputAction::KisAbstractInputAction(const PkString &id)
    : d(new Private)
{
    d->id = id;
    d->indexes.insert(PkString("Activate"), 0);
}

KisAbstractInputAction::~KisAbstractInputAction()
{
    delete d;
}

void KisAbstractInputAction::activate(int shortcut)
{
    (void)shortcut;
}

void KisAbstractInputAction::deactivate(int shortcut)
{
    (void)shortcut;
}

void KisAbstractInputAction::begin(int shortcut, PkInputEvent *event)
{
    (void)shortcut;

    if (event) {
        d->lastCursorPosition = eventPosF(event);
        d->startCursorPosition = d->lastCursorPosition;
    }
}

void KisAbstractInputAction::inputEvent(PkInputEvent *event)
{
    if (event) {
        PkPointF newPosition = eventPosF(event);
        cursorMoved(d->lastCursorPosition, newPosition);
        cursorMovedAbsolute(d->startCursorPosition, newPosition);
        d->lastCursorPosition = newPosition;
    }
}

void KisAbstractInputAction::end(PkInputEvent *event)
{
    (void)event;
}

void KisAbstractInputAction::cursorMoved(const PkPointF &lastPos, const PkPointF &pos)
{
    (void)lastPos;
    (void)pos;
}

void KisAbstractInputAction::cursorMovedAbsolute(const PkPointF &startPos, const PkPointF &pos)
{
    (void)startPos;
    (void)pos;
}

bool KisAbstractInputAction::supportsHiResInputEvents(int shortcut) const
{
    (void)shortcut;
    return false;
}

KisInputActionGroup KisAbstractInputAction::inputActionGroup(int shortcut) const
{
    (void)shortcut;
    return ModifyingActionGroup;
}

KisInputManager* KisAbstractInputAction::inputManager() const
{
    return Private::inputManager;
}

PkString KisAbstractInputAction::name() const
{
    return d->name;
}

PkString KisAbstractInputAction::description() const
{
    return d->description;
}

int KisAbstractInputAction::priority() const
{
    return 0;
}

bool KisAbstractInputAction::canIgnoreModifiers() const
{
    return false;
}

PkHash<PkString, int> KisAbstractInputAction::shortcutIndexes() const
{
    return d->indexes;
}

PkString KisAbstractInputAction::id() const
{
    return d->id;
}

void KisAbstractInputAction::setName(const PkString &name)
{
    d->name = name;
}

void KisAbstractInputAction::setDescription(const PkString &description)
{
    d->description = description;
}

void KisAbstractInputAction::setShortcutIndexes(const PkHash<PkString, int> &indexes)
{
    d->indexes = indexes;
}

void KisAbstractInputAction::setInputManager(KisInputManager *manager)
{
    Private::inputManager = manager;
}

void KisAbstractInputAction::setNativeGestureMapper(std::function<PkPointF(KisInputManager *, const PkNativeGestureEvent *)> mapper)
{
    Private::nativeGestureMapper = std::move(mapper);
}

bool KisAbstractInputAction::isShortcutRequired(int shortcut) const
{
    (void)shortcut;
    return false;
}

PkPoint KisAbstractInputAction::eventPos(const PkInputEvent *event)
{
    if(!event) {
        return PkPoint();
    }
    if (event->type() == PkInputEvent::NativeGesture) {
        if (!Private::nativeGestureMapper) return PkPoint();
        return Private::nativeGestureMapper(d->inputManager,
                                            static_cast<const PkNativeGestureEvent *>(event)).toPoint();
    }
    return event->localPosition().toPoint();
}

PkPointF KisAbstractInputAction::eventPosF(const PkInputEvent *event) {

    if(!event) {
        return PkPointF();
    }
    if (event->type() == PkInputEvent::NativeGesture) {
        if (!Private::nativeGestureMapper) return PkPointF();
        return Private::nativeGestureMapper(d->inputManager,
                                            static_cast<const PkNativeGestureEvent *>(event));
    }
    return event->localPosition();
}

bool KisAbstractInputAction::isAvailable() const
{
    return true;
}
