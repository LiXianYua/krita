/*
 *  SPDX-FileCopyrightText: 2015 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef STORE_DEBUG_H_
#define STORE_DEBUG_H_

#include <PkLoggingCategory.h>
#include <PkMessageLogger.h>
#include <kritastore_export.h>

extern const KRITASTORE_EXPORT PkLoggingCategory &STORE_LOG();

#define debugStore qCDebug(STORE_LOG)
#define warnStore qCWarning(STORE_LOG)
#define errorStore qCCritical(STORE_LOG)

#endif
