/*
 *  SPDX-FileCopyrightText: 2020 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef RESOURCE_DEBUG_H_
#define RESOURCE_DEBUG_H_

#include <PkLoggingCategory.h>
#include <PkMessageLogger.h>
#include <kritaresources_export.h>

extern const KRITARESOURCES_EXPORT PkLoggingCategory &RESOURCE_LOG();

#define debugResource qCDebug(RESOURCE_LOG)
#define warnResource qCWarning(RESOURCE_LOG)
#define errorResource qCCritical(RESOURCE_LOG)

#endif
