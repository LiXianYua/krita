/*
 *  SPDX-FileCopyrightText: 2020 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ResourceDebug.h"

const PkLoggingCategory &RESOURCE_LOG()
{
    static const PkLoggingCategory category("krita.lib.resource", PkLogInfo);
    return category;
}
