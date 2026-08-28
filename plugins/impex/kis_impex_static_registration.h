/*
 * SPDX-FileCopyrightText: 2026 S-09-c
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_IMPEX_STATIC_REGISTRATION_H
#define KIS_IMPEX_STATIC_REGISTRATION_H

#include "kis_impex_static_registration_once.h"

#include <KisImportExportManager.h>

#include <utility>

template<typename Factory>
bool registerKisImpexFilterOnce(bool &registered,
                                PkStringList importMimeTypes,
                                PkStringList exportMimeTypes,
                                int weight,
                                Factory &&factory)
{
    return invokeKisImpexRegistrationOnce(registered, [&] {
        KisImportExportManager::registerFilter({std::move(importMimeTypes),
                                                std::move(exportMimeTypes),
                                                weight,
                                                std::forward<Factory>(factory)});
    });
}

#endif
