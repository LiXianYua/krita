/*
 *  SPDX-FileCopyrightText: 2015 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KRITAVERSIONWRAPPER_H
#define KRITAVERSIONWRAPPER_H

#include "kritaversion_export.h"
#include "PkString.h"

namespace KritaVersionWrapper {

    KRITAVERSION_EXPORT PkString versionString(bool checkGit = false);
    KRITAVERSION_EXPORT bool isDevelopersBuild();
}

#endif // KRITAVERSIONWRAPPER_H
