/*
 *  SPDX-FileCopyrightText: 2025 Halla Rempt <halla@valdyas.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <KisMessageBoxWrapper.h>

namespace KisMessageBoxWrapper {

int doNotAskAgainMessageBoxWrapper(PkMessageBox *, const PkString &)
{
    // S-02-a: 剥离后空实现。PkMessageBox 不存在于内核；"Do Not Ask Again" 状态
    // 由 Flutter 侧负责。返回 QMessageBox::Yes（0x4000）等价默认值。
    return 16384;
}

}
