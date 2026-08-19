/*
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "KisDeleteLaterWrapper.h"

void KisDeleteLaterWrapperPrivate::moveToGuiThread(PkObject *object)
{
    object->moveToThread(qApp->thread());
}
