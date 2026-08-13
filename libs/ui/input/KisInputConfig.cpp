/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "KisInputConfig.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <KoPointerEvent.h>

bool KisInputConfig::disableTouchOnCanvas() const
{
    const int touchPainting = KSharedConfig::openConfig()->group("").readEntry("touchPainting", 0);

    if (touchPainting == 1) {
        return false;
    }
    if (touchPainting == 2) {
        return true;
    }
    return KoPointerEvent::tabletInputReceived();
}
