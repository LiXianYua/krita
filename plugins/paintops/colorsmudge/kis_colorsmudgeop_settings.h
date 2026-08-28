/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_COLORSMUDGEOP_SETTINGS_H
#define __KIS_COLORSMUDGEOP_SETTINGS_H

#include <PkList.h>
#include <PkPointer.h>
#include <PkScopedPointer.h>
#include <kis_brush_based_paintop_settings.h>


class KisColorSmudgeOpSettings : public KisBrushBasedPaintOpSettings
{
public:
    KisColorSmudgeOpSettings(KisResourcesInterfaceSP resourcesInterface);
    ~KisColorSmudgeOpSettings() override;

    PkList<KisUniformPaintOpPropertySP> uniformProperties(KisPaintOpSettingsSP settings, PkPointer<KisPaintOpPresetUpdateProxy> updateProxy) override;

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

typedef KisSharedPtr<KisColorSmudgeOpSettings> KisColorSmudgeOpSettingsSP;

#endif /* __KIS_COLORSMUDGEOP_SETTINGS_H */
