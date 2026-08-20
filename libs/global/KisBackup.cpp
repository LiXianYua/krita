#include <PkString.h>



#include <QLatin1Char>
/*
    This file is part of the KDE libraries

    SPDX-FileCopyrightText: 1999 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2006 Allen Winter <winter@kde.org>
    SPDX-FileCopyrightText: 2006 Gregory S. Hayes <syncomm@kde.org>
    SPDX-FileCopyrightText: 2006 Jaison Lee <lee.jaison@gmail.com>
    SPDX-FileCopyrightText: 2011 Romain Perier <bambi@ubuntu.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "KisBackup.h"

#include <PkDebug.h>

bool KisBackup::backupFile(const PkString &qFilename, const PkString &backupDir)
{
    return (simpleBackupFile(qFilename, backupDir, PkString("~")));
}

bool KisBackup::simpleBackupFile(const PkString &qFilename, const PkString &backupDir, const PkString &backupExtension)
{
    PkString backupFileName = qFilename + backupExtension;

    if (!backupDir.isEmpty()) {
        PkFileInfo fileInfo(qFilename);
        backupFileName = backupDir + QLatin1Char('/') + fileInfo.fileName() + backupExtension;
    }

    //    qCDebug(KCOREADDONS_DEBUG) << "KisBackup copying " << qFilename << " to " << backupFileName;
    PkFile::remove(backupFileName);
    return PkFile::copy(qFilename, backupFileName);
}

bool KisBackup::numberedBackupFile(const PkString &qFilename, const PkString &backupDir, const PkString &backupExtension, const uint maxBackups)
{
    PkFileInfo fileInfo(qFilename);

    // The backup file name template.
    PkString sTemplate;

    if (backupDir.isEmpty()) {
        sTemplate = qFilename + QLatin1String(".%1") + backupExtension;
    } else {
        sTemplate = backupDir + QLatin1Char('/') + fileInfo.fileName() + QLatin1String(".%1") + backupExtension;
    }
    // First, search backupDir for numbered backup files to remove.
    // Remove all with number 'maxBackups' and greater.
    PkDir d = backupDir.isEmpty() ? fileInfo.dir() : backupDir;
    d.setFilter(PkDir::Files | PkDir::Hidden | PkDir::NoSymLinks);

    PkString nameFilter = fileInfo.fileName() + QLatin1String(".*") + backupExtension;
    nameFilter.replace('[', '*');
    nameFilter.replace(']', '*');

    const PkStringList nameFilters = PkStringList(nameFilter);
    d.setNameFilters(nameFilters);
    d.setSorting(PkDir::Name);

    uint maxBackupFound = 0;
    const PkFileInfoList infoList = d.entryInfoList();
    for (const PkFileInfo &fi : infoList) {
        if (fi.fileName().endsWith(backupExtension)) {
            // sTemp holds the file name, without the ending backupExtension
            PkString sTemp = fi.fileName();

            sTemp.truncate(fi.fileName().length() - backupExtension.length());

            // compute the backup number
            int idex = sTemp.lastIndexOf(QLatin1Char('.'));
            if (idex > 0) {
                bool ok;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                const uint num = PkStringView(sTemp).mid(idex + 1).toUInt(&ok);
#else
                const uint num = sTemp.midRef(idex + 1).toUInt(&ok);
#endif
                if (ok) {
                    if (num >= maxBackups) {
                        PkFile::remove(fi.filePath());
                    } else {
                        maxBackupFound = qMax(maxBackupFound, num);
                    }
                }
            }
        }
    }

    // Next, rename max-1 to max, max-2 to max-1, etc.
    PkString to = sTemplate.arg(maxBackupFound + 1);

    for (int i = maxBackupFound; i > 0; i--) {
        PkString from = sTemplate.arg(i);
        //        qCDebug(KCOREADDONS_DEBUG) << "KisBackup renaming " << from << " to " << to;
        PkFile::rename(from, to);
        to = from;
    }

    // Finally create most recent backup by copying the file to backup number 1.
    //    qCDebug(KCOREADDONS_DEBUG) << "KisBackup copying " << qFilename << " to " << sTemplate.arg(1);
    bool r = PkFile::copy(qFilename, sTemplate.arg(1));
    return r;
}
