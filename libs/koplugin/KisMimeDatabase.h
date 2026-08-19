/*
 * SPDX-FileCopyrightText: 2016 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#ifndef KISMIMEDATABASE_H
#define KISMIMEDATABASE_H

#include "PkString.h"
#include "PkStringList.h"

#include "kritaplugin_export.h"

class PkByteArray;

/**
 * @brief The KisMimeDatabase class maps file extensions to mimetypes and vice versa
 */
class KRITAPLUGIN_EXPORT KisMimeDatabase
{
public:

    /// Find the mimetype for the given filename. The filename must include a suffix.
    static PkString mimeTypeForFile(const PkString &file, bool checkExistingFiles = true);
    /// Find the mimetype for a given extension. The extension may have the form "*.xxx" or "xxx"
    static PkString mimeTypeForSuffix(const PkString &suffix);
    /// Find the mimetype through analyzing the contents. This does not work for Krita's
    /// extended mimetypes.
    static PkString mimeTypeForData(const PkByteArray &ba);
    /// Find the user-readable description for the given mimetype
    static PkString descriptionForMimeType(const PkString &mimeType);
    /// Find the list of possible extensions for the given mimetype.
    /// The preferred suffix is the first one.
    static PkStringList suffixesForMimeType(const PkString &mimeType);

};

#endif // KISMIMEDATABASE_H
