/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisBrushOpSettings.h"

struct KisBrushOpSettings::Private
{
    PkList<KisUniformPaintOpPropertyWSP> uniformProperties;
};

KisBrushOpSettings::KisBrushOpSettings(KisResourcesInterfaceSP resourcesInterface)
    : KisBrushBasedPaintOpSettings(resourcesInterface),
      m_d(new Private)
{
}

KisBrushOpSettings::~KisBrushOpSettings()
{
}

bool KisBrushOpSettings::needsAsynchronousUpdates() const
{
    return true;
}

#include "kis_paintop_preset.h"
#include "KisPaintOpPresetUpdateProxy.h"
#include "KisCurveOptionDataUniformProperty.h"
#include "KisStandardOptionData.h"

PkList<KisUniformPaintOpPropertySP> KisBrushOpSettings::uniformProperties(KisPaintOpSettingsSP settings, PkPointer<KisPaintOpPresetUpdateProxy> updateProxy)
{
    PkList<KisUniformPaintOpPropertySP> props = listWeakToStrong(m_d->uniformProperties);

    if (props.isEmpty()) {
        {
            KisCurveOptionDataUniformProperty *prop =
                new KisCurveOptionDataUniformProperty(
                    KisLightnessStrengthOptionData(),
                    "lightness_strength",
                    settings, 0);

            PkObject::connect(updateProxy, &KisPaintOpPresetUpdateProxy::sigSettingsChanged,
                              prop, &KisUniformPaintOpProperty::requestReadValue);
            prop->requestReadValue();
            props << toQShared(prop);
        }
    }

    PkList<KisUniformPaintOpPropertySP> base =
        KisBrushBasedPaintOpSettings::uniformProperties(settings, updateProxy);
    base.append(props);
    return base;
}
