/*
 *  SPDX-FileCopyrightText: 2010 Lukáš Tvrdý <lukast.dev@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_PARTICLE_PAINTOP_SETTINGS_H_
#define KIS_PARTICLE_PAINTOP_SETTINGS_H_

#include <PkScopedPointer.h>
#include <brushengine/kis_no_size_paintop_settings.h>
#include <kis_types.h>

class KisParticlePaintOpSettings : public KisNoSizePaintOpSettings
{

public:

    KisParticlePaintOpSettings(KisResourcesInterfaceSP resourcesInterface);
    ~KisParticlePaintOpSettings() override;

    bool paintIncremental() override;

    PkList<KisUniformPaintOpPropertySP> uniformProperties(KisPaintOpSettingsSP settings, PkPointer<KisPaintOpPresetUpdateProxy> updateProxy) override;

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif
