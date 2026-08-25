/*
 *  SPDX-FileCopyrightText: 2019 Agata Cacko <cacko.azh@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_IMPORT_EXPORT_ADDITIONAL_CHECKS_H
#define KIS_IMPORT_EXPORT_ADDITIONAL_CHECKS_H

#include <PkString.h>
#include <KisImportExportErrorCode.h>

class KRITAIMPEX_EXPORT KisImportExportAdditionalChecks
{

public:

    static bool isFileWritable(PkString filepath);
    static bool isFileReadable(PkString filepath);
    static bool doesFileExist(PkString filepath);
};




#endif // #ifndef KIS_IMPORT_EXPORT_ADDITIONAL_CHECKS_H
