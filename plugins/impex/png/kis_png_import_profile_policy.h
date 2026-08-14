/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_PNG_IMPORT_PROFILE_POLICY_H
#define KIS_PNG_IMPORT_PROFILE_POLICY_H

#include <KisPngCodec.h>

class KisPngImportProfileDesktopPolicy final : public KisPngImportProfilePolicy
{
public:
    explicit KisPngImportProfileDesktopPolicy(bool batchMode);

    QString chooseColorProfile(const KisPngImportProfileRequest &request) override;

private:
    const bool m_batchMode;
};

#endif // KIS_PNG_IMPORT_PROFILE_POLICY_H
