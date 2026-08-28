/*
 * SPDX-FileCopyrightText: 2015 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#ifndef KIS_DLG_PNG_IMPORT_H
#define KIS_DLG_PNG_IMPORT_H

#include <PkString.h>
#include <PkStringList.h>


class KisDlgPngImport
{
public:
    KisDlgPngImport(const PkString &path,
                    const PkString &colorModelId,
                    const PkString &colorDepthId);

    const PkString &sourcePath() const;
    const PkStringList &profiles() const;
    bool selectProfile(const PkString &profile);
    PkString profile() const;

private:
    PkString m_sourcePath;
    PkStringList m_profiles;
    PkString m_selectedProfile;
};

#endif // KIS_DLG_PNG_IMPORT_H
