/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include <KisResourceLoader.h>
#include <KisMimeDatabase.h>

/**
 * @return a set of filters ("*.bla,*.foo") that is suitable for filtering
 * the contents of a directory.
 */
PkStringList KisResourceLoaderBase::filters() const
{
    PkStringList filters;
    const PkStringList mimeTypes = mimetypes();
    for (const PkString &mimeType : mimeTypes) {
        const PkStringList suffixes = KisMimeDatabase::suffixesForMimeType(mimeType);
        for (const PkString &suffix : suffixes) {
                filters << PkString("*.") + suffix;
        }
    }

    return filters;
}
