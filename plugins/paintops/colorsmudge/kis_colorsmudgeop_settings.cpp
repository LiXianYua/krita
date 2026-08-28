/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_colorsmudgeop_settings.h"

struct KisColorSmudgeOpSettings::Private
{
    PkList<KisUniformPaintOpPropertyWSP> uniformProperties;
};

KisColorSmudgeOpSettings::KisColorSmudgeOpSettings(KisResourcesInterfaceSP resourcesInterface)
    : KisBrushBasedPaintOpSettings(resourcesInterface),
      m_d(new Private)
{
}

KisColorSmudgeOpSettings::~KisColorSmudgeOpSettings()
{
}

#include <brushengine/kis_slider_based_paintop_property.h>
#include <brushengine/kis_combo_based_paintop_property.h>
#include "kis_paintop_preset.h"
#include "KisPaintOpPresetUpdateProxy.h"
#include "KisSmudgeLengthOptionData.h"
#include "KisPaintThicknessOptionData.h"
#include "KisColorSmudgeStandardOptionData.h"
#include "KisCurveOptionDataUniformProperty.h"
#include "KisSmudgeRadiusOptionData.h"

PkList<KisUniformPaintOpPropertySP> KisColorSmudgeOpSettings::uniformProperties(KisPaintOpSettingsSP settings, PkPointer<KisPaintOpPresetUpdateProxy> updateProxy)
{
    PkList<KisUniformPaintOpPropertySP> props =
        listWeakToStrong(m_d->uniformProperties);

    if (props.isEmpty()) {
        {
            KisComboBasedPaintOpPropertyCallback *prop = new KisComboBasedPaintOpPropertyCallback(KoID("smudge_mode", "Smudge Mode"), settings, 0);

            PkList<PkString> modes;
            modes << "Smearing";
            modes << "Dulling";

            prop->setItems(modes);

            prop->setReadCallback(
                [](KisUniformPaintOpProperty *prop) {
                    KisSmudgeLengthOptionData data;
                    data.read(prop->settings().data());
                    prop->setValue(int(data.mode));
                });
            prop->setWriteCallback(
                [](KisUniformPaintOpProperty *prop) {
                    KisSmudgeLengthOptionData data;
                    data.read(prop->settings().data());
                    data.mode = KisSmudgeLengthOptionData::Mode(prop->value().toInt());
                    data.write(prop->settings().data());
                });

            PkObject::connect(updateProxy, &KisPaintOpPresetUpdateProxy::sigSettingsChanged,
                              prop, &KisUniformPaintOpProperty::requestReadValue);
            prop->requestReadValue();
            props << toQShared(prop);
        }

        {
            KisCurveOptionDataUniformProperty *prop =
                new KisCurveOptionDataUniformProperty(
                    KisSmudgeLengthOptionData(),
                    "smudge_length",
                    settings, 0);

            PkObject::connect(updateProxy, &KisPaintOpPresetUpdateProxy::sigSettingsChanged,
                              prop, &KisUniformPaintOpProperty::requestReadValue);
            prop->requestReadValue();
            props << toQShared(prop);
        }
        {
            KisCurveOptionDataUniformProperty *prop =
                new KisCurveOptionDataUniformProperty(
                    KisSmudgeRadiusOptionData(),
                    "smudge_radius",
                    settings, 0);

            PkObject::connect(updateProxy, &KisPaintOpPresetUpdateProxy::sigSettingsChanged,
                              prop, &KisUniformPaintOpProperty::requestReadValue);
            prop->requestReadValue();
            props << toQShared(prop);
        }

        {
            KisCurveOptionDataUniformProperty *prop =
                new KisCurveOptionDataUniformProperty(
                        KisColorRateOptionData(),
                        "smudge_color_rate",
                        settings, 0);

            PkObject::connect(updateProxy, &KisPaintOpPresetUpdateProxy::sigSettingsChanged,
                              prop, &KisUniformPaintOpProperty::requestReadValue);
            prop->requestReadValue();
            props << toQShared(prop);
        }

        {
            KisUniformPaintOpPropertyCallback *prop =
                new KisUniformPaintOpPropertyCallback(KisUniformPaintOpPropertyCallback::Bool, KoID("smudge_smear_alpha", "Smear Alpha"), settings, 0);

            prop->setReadCallback(
                [](KisUniformPaintOpProperty *prop) {
                    KisSmudgeLengthOptionData data;
                    data.read(prop->settings().data());

                    prop->setValue(data.smearAlpha);
                });
            prop->setWriteCallback(
                [](KisUniformPaintOpProperty *prop) {
                    KisSmudgeLengthOptionData data;
                    data.read(prop->settings().data());
                    data.smearAlpha = prop->value().toBool();
                    data.write(prop->settings().data());
                });

            PkObject::connect(updateProxy, &KisPaintOpPresetUpdateProxy::sigSettingsChanged,
                              prop, &KisUniformPaintOpProperty::requestReadValue);
            prop->requestReadValue();
            props << toQShared(prop);
        }

        {
            KisCurveOptionDataUniformProperty *prop =
                new KisCurveOptionDataUniformProperty(
                    KisPaintThicknessOptionData(),
                    "smudge_paint_thickness_rate",
                    settings, 0);

            PkObject::connect(updateProxy, &KisPaintOpPresetUpdateProxy::sigSettingsChanged,
                              prop, &KisUniformPaintOpProperty::requestReadValue);
            prop->requestReadValue();
            props << toQShared(prop);
        }

        {
            KisComboBasedPaintOpPropertyCallback *prop =
                new KisComboBasedPaintOpPropertyCallback(KoID("smudge_paint_thickness_mode", "Paint Thickness Mode"), settings, 0);

            PkList<PkString> modes;
            modes << "Overwrite";
            modes << "Paint over";

            prop->setItems(modes);

            prop->setReadCallback(
                [](KisUniformPaintOpProperty *prop) {
                    KisPaintThicknessOptionData data;
                    data.read(prop->settings().data());

                    prop->setValue(int(data.mode) - 1);
                });
            prop->setWriteCallback(
                [](KisUniformPaintOpProperty *prop) {
                    KisPaintThicknessOptionData data;
                    data.read(prop->settings().data());
                    data.mode = KisPaintThicknessOptionData::ThicknessMode(prop->value().toInt() + 1);
                    data.write(prop->settings().data());
                });

            PkObject::connect(updateProxy, &KisPaintOpPresetUpdateProxy::sigSettingsChanged,
                              prop, &KisUniformPaintOpProperty::requestReadValue);
            prop->requestReadValue();
            props << toQShared(prop);
        }
    }

    PkList<KisUniformPaintOpPropertySP> result =
        KisBrushBasedPaintOpSettings::uniformProperties(settings, updateProxy);
    result.append(props);
    return result;
}
