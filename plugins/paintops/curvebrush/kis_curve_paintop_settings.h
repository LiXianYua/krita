/*
 *  SPDX-FileCopyrightText: 2008 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2008 Lukas Tvrdy <lukast.dev@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_CURVE_PAINTOP_SETTINGS_H_
#define KIS_CURVE_PAINTOP_SETTINGS_H_

#include <PkScopedPointer.h>
#include <brushengine/kis_paintop_settings.h>

class KisCurvePaintOpSettings : public KisPaintOpSettings
{

public:
    KisCurvePaintOpSettings(KisResourcesInterfaceSP resourcesInterface);
    ~KisCurvePaintOpSettings() override;

    void setPaintOpSize(qreal value) override;
    qreal paintOpSize() const override;

    void setPaintOpAngle(qreal value) override;
    qreal paintOpAngle() const override;

    bool paintIncremental() override;

    PkList<KisUniformPaintOpPropertySP> uniformProperties(KisPaintOpSettingsSP settings, PkPointer<KisPaintOpPresetUpdateProxy> updateProxy) override;

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};
#endif
