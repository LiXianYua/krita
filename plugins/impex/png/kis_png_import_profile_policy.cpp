/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_png_import_profile_policy.h"

#include "kis_dlg_png_import.h"

KisPngImportProfileDesktopPolicy::KisPngImportProfileDesktopPolicy(bool batchMode)
{
    (void)batchMode;
}

PkString KisPngImportProfileDesktopPolicy::chooseColorProfile(
    const KisPngImportProfileRequest &request)
{
    KisDlgPngImport model(request.sourcePath,
                         request.colorModelId,
                         request.colorDepthId);
    return model.profile();
}
