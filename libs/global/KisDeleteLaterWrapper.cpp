/*
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "KisDeleteLaterWrapper.h"

void KisDeleteLaterWrapperPrivate::moveToGuiThread(QObject *object)
{
    object->moveToThread(qApp->thread());
}
