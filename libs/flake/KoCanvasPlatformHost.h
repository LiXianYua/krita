/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 *
 * Platform-query contract between retained flake code and the UI host.
 */
#ifndef KOCANVASPLATFORMHOST_H
#define KOCANVASPLATFORMHOST_H

#include "kritaflake_export.h"

class KRITAFLAKE_EXPORT KoCanvasPlatformHost
{
public:
    virtual ~KoCanvasPlatformHost() = default;

    /** 宿主平台的鼠标双击判定间隔（毫秒）。缺省 400 = Qt 的缺省值。 */
    virtual int doubleClickInterval() const { return 400; }
};

#endif // KOCANVASPLATFORMHOST_H
