/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 *
 * Platform-query contract between retained flake code and the UI host.
 */
#ifndef KOCANVASPLATFORMHOST_H
#define KOCANVASPLATFORMHOST_H

#include <PkColor.h>

#include "kritaflake_export.h"

class KRITAFLAKE_EXPORT KoCanvasPlatformHost
{
public:
    virtual ~KoCanvasPlatformHost() = default;

    /** 宿主平台的鼠标双击判定间隔（毫秒）。缺省 400 = Qt 的缺省值。 */
    virtual int doubleClickInterval() const { return 400; }

    /** 平台的光标宽度（像素）。缺省 1 = Qt QCommonStyle::PM_TextCursorWidth 的缺省值。 */
    virtual int textCursorWidth() const { return 1; }
    /** 平台的光标闪烁周期（毫秒）。缺省 1000 = QApplication::cursorFlashTime() 的缺省值。 */
    virtual int cursorFlashTime() const { return 1000; }
    /** 选区存在时游标是否仍闪烁。缺省 false = Qt QCommonStyle::SH_BlinkCursorWhenTextSelected 的缺省值。 */
    virtual bool blinkCursorWhenTextSelected() const { return false; }
    /** 选区高亮色。缺省 `PkColor()`（Invalid）= 「宿主没给」，与「宿主给了个透明色」可区分。 */
    virtual PkColor highlightColor() const { return PkColor(); }
};

#endif // KOCANVASPLATFORMHOST_H
