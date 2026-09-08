/*
 *  This file is part of the KDE project
 *  SPDX-FileCopyrightText: 2012 Arjen Hiemstra <ahiemstra@heimr.nl>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#include "kis_touch_shortcut.h"
#include "kis_abstract_input_action.h"
#include "KisInputConfig.h"

class KisTouchShortcut::Private
{
public:
    Private(GestureAction type)
        : minTouchPoints(0)
        , maxTouchPoints(0)
        , type(type)
        , disableOnTouchPainting(false)
    { }

    int minTouchPoints;
    int maxTouchPoints;
    GestureAction type;
    bool disableOnTouchPainting;
};

KisTouchShortcut::KisTouchShortcut(KisAbstractInputAction* action, int index, GestureAction type)
    : KisAbstractShortcut(action, index)
    , d(new Private(type))
{

}

KisTouchShortcut::~KisTouchShortcut()
{
    delete d;
}

int KisTouchShortcut::priority() const
{
    return action()->priority();
}

bool KisTouchShortcut::isHoldType() const
{
#if defined(__APPLE__) && defined(__MACH__) && defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__)
    return false; // No equivalent gestures on macOS.
#else
    return d->type == KisShortcutConfiguration::OneFingerHold;
#endif
}

void KisTouchShortcut::setMinimumTouchPoints(int min)
{
    d->minTouchPoints = min;
}

void KisTouchShortcut::setMaximumTouchPoints(int max)
{
    d->maxTouchPoints = max;
}

void KisTouchShortcut::setDisableOnTouchPainting(bool disableOnTouchPainting)
{
    d->disableOnTouchPainting = disableOnTouchPainting;
}

bool KisTouchShortcut::matchTapType(PkTouchEvent *event)
{
    return matchTouchPoint(event)
#if !defined(__APPLE__) || !defined(__MACH__) || !defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__)
        && (d->type >= KisShortcutConfiguration::OneFingerTap && d->type <= KisShortcutConfiguration::FiveFingerTap)
#endif
        ;
}

bool KisTouchShortcut::matchDragType(PkTouchEvent *event)
{
    return matchTouchPoint(event)
#if !defined(__APPLE__) || !defined(__MACH__) || !defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__)
        && (d->type >= KisShortcutConfiguration::OneFingerDrag && d->type <= KisShortcutConfiguration::FiveFingerDrag)
#endif
        ;
}

bool KisTouchShortcut::matchHoldType(PkTouchEvent *event)
{
    return isHoldType() && matchTouchPoint(event);
}

bool KisTouchShortcut::matchTouchPoint(PkTouchEvent *event)
{
    return (!d->disableOnTouchPainting || KisInputConfig().disableTouchOnCanvas())
        && event->touchPoints().count() >= d->minTouchPoints && event->touchPoints().count() <= d->maxTouchPoints;
}
