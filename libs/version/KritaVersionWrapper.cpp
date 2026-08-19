/*
 *  SPDX-FileCopyrightText: 2015 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KritaVersionWrapper.h"

#include <kritaversion.h>
#include <kritagitversion.h>

PkString KritaVersionWrapper::versionString(bool checkGit)
{
    PkString kritaVersion = PkString(KRITA_VERSION_STRING);
    PkString version = kritaVersion;

    if (checkGit) {
#ifdef KRITA_GIT_SHA1_STRING
        PkString gitVersion = PkString(KRITA_GIT_SHA1_STRING);
        version = PkString("%1 (git %2)").arg(kritaVersion, gitVersion);
#endif
    }
    return version;
}

bool KritaVersionWrapper::isDevelopersBuild()
{
#if defined(KRITA_STABLE)
    return false;
#else
    return true;
#endif
}
