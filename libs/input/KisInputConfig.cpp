/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "KisInputConfig.h"

#include <PkConfigGroup.h>
#include <PkSharedConfig.h>
#include <PkString.h>

bool KisInputConfig::disableTouchOnCanvas() const
{
    const int touchPainting = PkSharedConfig::openConfig()->group(PkString("")).readEntry(PkString("touchPainting"), 0);

    if (touchPainting == 1) {
        return false;
    }
    if (touchPainting == 2) {
        return true;
    }
    // KoPointerEvent::tabletInputReceived() 桩化：零调用方死代码，真实 tablet-input
    // 状态归 S-08-b/flake 的 GAP 侧（S-08-a brief 上下文 7 显式接受）。
    static bool s_tabletInputReceived = false;
    return s_tabletInputReceived;
}
