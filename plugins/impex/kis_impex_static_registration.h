/*
 * SPDX-FileCopyrightText: 2026 S-09-c
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_IMPEX_STATIC_REGISTRATION_H
#define KIS_IMPEX_STATIC_REGISTRATION_H

#include <KisImportExportManager.h>

#include <utility>

template<typename Factory>
void registerKisImpexFilterOnce(bool &registered,
                                PkStringList importMimeTypes,
                                PkStringList exportMimeTypes,
                                int weight,
                                Factory &&factory)
{
    if (registered) {
        return;
    }
    registered = true;
    KisImportExportManager::registerFilter({std::move(importMimeTypes),
                                            std::move(exportMimeTypes),
                                            weight,
                                            std::forward<Factory>(factory)});
}

#endif
