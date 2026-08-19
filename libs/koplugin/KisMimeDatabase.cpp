/*
 * SPDX-FileCopyrightText: 2016 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include "KisMimeDatabase.h"

#include "PkMimeDatabase.h"

PkString KisMimeDatabase::mimeTypeForFile(const PkString &file, bool checkExistingFiles)
{
    return PkMimeDatabase::mimeTypeForFile(file, checkExistingFiles);
}

PkString KisMimeDatabase::mimeTypeForSuffix(const PkString &suffix)
{
    return PkMimeDatabase::mimeTypeForSuffix(suffix);
}

PkString KisMimeDatabase::mimeTypeForData(const PkByteArray &ba)
{
    return PkMimeDatabase::mimeTypeForData(ba);
}

PkString KisMimeDatabase::descriptionForMimeType(const PkString &mimeType)
{
    return PkMimeDatabase::descriptionForMimeType(mimeType);
}

PkStringList KisMimeDatabase::suffixesForMimeType(const PkString &mimeType)
{
    return PkMimeDatabase::suffixesForMimeType(mimeType);
}
