/*
 *  SPDX-FileCopyrightText: 2019 Agata Cacko <cacko.azh@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisImportExportAdditionalChecks.h"

#include <unistd.h>
#include <filesystem>


bool KisImportExportAdditionalChecks::isFileWritable(PkString filepath)
{
    return ::access(filepath.PkToUtf8().c_str(), W_OK) == 0;
}

bool KisImportExportAdditionalChecks::isFileReadable(PkString filepath)
{
    return ::access(filepath.PkToUtf8().c_str(), R_OK) == 0;
}

bool KisImportExportAdditionalChecks::doesFileExist(PkString filepath)
{
    return std::filesystem::exists(filepath.PkToUtf8());
}
