/*
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "KisDeleteLaterWrapper.h"

#include <PkThread.h>

void KisDeleteLaterWrapperPrivate::moveToGuiThread(PkObject *object)
{
    object->moveToThread(PkThread::mainThreadId());
}
