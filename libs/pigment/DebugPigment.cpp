/*
 *  SPDX-FileCopyrightText: 2015 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DebugPigment.h"
const PkLoggingCategory &PIGMENT_log()
{
    static const PkLoggingCategory category("krita.lib.pigment", PkLogInfo);
    return category;
}


