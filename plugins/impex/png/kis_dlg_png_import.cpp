/*
 * SPDX-FileCopyrightText: 2015 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include "kis_dlg_png_import.h"

#include <algorithm>

#include <PkConfigGroup.h>
#include <PkSharedConfig.h>

#include <KoColorProfile.h>
#include <KoColorSpaceRegistry.h>

KisDlgPngImport::KisDlgPngImport(const PkString &path,
                                 const PkString &colorModelId,
                                 const PkString &colorDepthId)
    : m_sourcePath(path)
{
    KoColorSpaceRegistry *registry = KoColorSpaceRegistry::instance();
    const PkString colorSpaceId = registry->colorSpaceId(colorModelId, colorDepthId);
    const PkList<const KoColorProfile *> profileList = registry->profilesFor(colorSpaceId);
    for (const KoColorProfile *profile : profileList) {
        if (profile) {
            m_profiles.append(profile->name());
        }
    }
    std::sort(m_profiles.begin(), m_profiles.end());

    const PkString defaultProfile = registry->defaultProfileForColorSpace(colorSpaceId);
    const PkConfigGroup config = PkSharedConfig::openConfig()->group(PkString());
    const PkString persistedProfile =
        config.readEntry(PkString("pngImportProfile"), defaultProfile);

    if (std::find(m_profiles.begin(), m_profiles.end(), persistedProfile) != m_profiles.end()) {
        m_selectedProfile = persistedProfile;
    } else if (std::find(m_profiles.begin(), m_profiles.end(), defaultProfile) != m_profiles.end()) {
        m_selectedProfile = defaultProfile;
    } else if (!m_profiles.isEmpty()) {
        m_selectedProfile = m_profiles.front();
    }
}

const PkString &KisDlgPngImport::sourcePath() const
{
    return m_sourcePath;
}

const PkStringList &KisDlgPngImport::profiles() const
{
    return m_profiles;
}

bool KisDlgPngImport::selectProfile(const PkString &profile)
{
    if (std::find(m_profiles.begin(), m_profiles.end(), profile) == m_profiles.end()) {
        return false;
    }
    m_selectedProfile = profile;
    return true;
}

PkString KisDlgPngImport::profile() const
{
    PkConfigGroup config = PkSharedConfig::openConfig()->group(PkString());
    config.writeEntry(PkString("pngImportProfile"), m_selectedProfile);
    config.sync();
    return m_selectedProfile;
}
