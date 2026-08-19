/*
 *  SPDX-FileCopyrightText: 2019 Dmitry Kazakov <dimula73@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "KisFileUtils.h"

#include <PkString.h>

#include <KisPortingUtils.h>

namespace KritaUtils {

PkString resolveAbsoluteFilePath(const PkString &baseDir, const PkString &fileName)
{
    if (PkFileInfo(fileName).isAbsolute()) {
        return fileName;
    }

    PkFileInfo fallbackBaseDirInfo(baseDir);

    return PkFileInfo(PkDir(fallbackBaseDirInfo.isDir() ?
                              fallbackBaseDirInfo.absoluteFilePath() :
                              fallbackBaseDirInfo.absolutePath()),
                     fileName).absoluteFilePath();
}

PkString deduplicateFileName(const PkString &fileName,
                            const PkString &separator,
                            std::function<bool(PkString)> fileAllowedCallback)
{
    const PkFileInfo fileInfo(fileName);

    int counter = 0;
    PkString proposedFileName = fileInfo.fileName();

    PkString baseName = fileInfo.baseName();
    PkString completeSuffix = fileInfo.completeSuffix();

    /**
     * Search for the separator around the leftmost dot in the filename
     * and try to reuse its counter.
     *
     * The design choice is that there cannot be any dots to the left
     * from the separator. Separator itself can have dots, but it cannot
     * be a part of the file extension.
     */
    PkRegularExpression rex(PkString("^([^.]+)%1\\d+(\\.(.+))?$").arg(separator));
    auto match = rex.match(proposedFileName);

    if (match.hasMatch()) {
        using KisPortingUtils::stringRemoveFirst;
        baseName = match.captured(1);
        completeSuffix = stringRemoveFirst(match.captured(2));
    }

    while (!fileAllowedCallback(proposedFileName)) {
        PkStringList fileParts = {baseName, separator, PkString::number(counter++)};

        if (!completeSuffix.isEmpty()) {
            fileParts += ".";
            fileParts += completeSuffix;
        }
        proposedFileName = fileParts.join("");
    }

    return proposedFileName;
}
}
