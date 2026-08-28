#include <PkString.h>
#include <PkList.h>
#include <PkScopedPointer.h>
#include <PkPointer.h>
#include <PkPoint.h>
/*
 *  SPDX-FileCopyrightText: 2008, 2009, 2010 Lukáš Tvrdý <lukast.dev@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_SPRAY_PAINTOP_SETTINGS_H_
#define KIS_SPRAY_PAINTOP_SETTINGS_H_

#include <PkScopedPointer.h>

#include <brushengine/kis_no_size_paintop_settings.h>
#include <kis_types.h>

#include <kis_outline_generation_policy.h>


class KisSprayPaintOpSettings : public KisOutlineGenerationPolicy<KisPaintOpSettings>
{
public:
    KisSprayPaintOpSettings(KisResourcesInterfaceSP resourcesInterface);
    ~KisSprayPaintOpSettings() override;

    void setPaintOpSize(qreal value) override;
    qreal paintOpSize() const override;

    void setPaintOpAngle(qreal value) override;
    qreal paintOpAngle() const override;

    KisOptimizedBrushOutline brushOutline(const KisPaintInformation &info, const OutlineMode &mode, qreal alignForZoom) override;

    PkString modelName() const override {
        return "airbrush";
    }

    bool paintIncremental() override;

protected:

    PkList<KisUniformPaintOpPropertySP> uniformProperties(KisPaintOpSettingsSP settings, PkPointer<KisPaintOpPresetUpdateProxy> updateProxy) override;

private:
    Q_DISABLE_COPY(KisSprayPaintOpSettings)

    struct Private;
    const PkScopedPointer<Private> m_d;

};

typedef KisSharedPtr<KisSprayPaintOpSettings> KisSprayPaintOpSettingsSP;

#endif
