/*
 *  SPDX-FileCopyrightText: 2015 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "StoreDebug.h"

const PkLoggingCategory &STORE_LOG()
{
    static const PkLoggingCategory category("krita.lib.store", PkLogInfo);
    return category;
}
