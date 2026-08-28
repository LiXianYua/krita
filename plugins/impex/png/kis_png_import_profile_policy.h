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
    // Flutter and future UI layers inject profile selection through this
    // policy; the default implementation remains toolkit-free.
    explicit KisPngImportProfileDesktopPolicy(bool batchMode);

    PkString chooseColorProfile(const KisPngImportProfileRequest &request) override;
};

#endif // KIS_PNG_IMPORT_PROFILE_POLICY_H
