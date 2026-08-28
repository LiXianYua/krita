/*
 *  SPDX-FileCopyrightText: 2009, 2010 Lukáš Tvrdý <lukast.dev@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_experiment_paintop_settings.h"
#include "kis_current_outline_fetcher.h"
#include "kis_algebra_2d.h"
#include <KisOptimizedBrushOutline.h>

struct KisExperimentPaintOpSettings::Private
{
    PkList<KisUniformPaintOpPropertyWSP> uniformProperties;
};

KisExperimentPaintOpSettings::KisExperimentPaintOpSettings(KisResourcesInterfaceSP resourcesInterface)
    : KisNoSizePaintOpSettings(resourcesInterface),
      m_d(new Private)
{
}

KisExperimentPaintOpSettings::~KisExperimentPaintOpSettings()
{
}

bool KisExperimentPaintOpSettings::paintIncremental()
{
    /**
     * The experiment brush supports working in the
     * WASH mode only!
     */
    return false;
}

KisOptimizedBrushOutline KisExperimentPaintOpSettings::brushOutline(const KisPaintInformation &info, const OutlineMode &mode, qreal alignForZoom)
{
    PkPainterPath path;
    if (mode.isVisible) {

        PkRectF ellipse(0, 0, 3, 3);
        ellipse.translate(-ellipse.center());
        path.addEllipse(ellipse);
        ellipse.setRect(0,0, 12, 12);
        ellipse.translate(-ellipse.center());
        path.addEllipse(ellipse);

        if (mode.showTiltDecoration) {
            path.addPath(makeTiltIndicator(info, PkPointF(0.0, 0.0), 0.0, 3.0));
        }

        path.translate(KisAlgebra2D::alignForZoom(info.pos(), alignForZoom));
    }
    return path;
}

#include <brushengine/kis_slider_based_paintop_property.h>
#include "kis_paintop_preset.h"
#include "KisPaintOpPresetUpdateProxy.h"
#include "kis_standard_uniform_properties_factory.h"
#include <KisOptimizedBrushOutline.h>
#include "KisExperimentOpOptionData.h"



PkList<KisUniformPaintOpPropertySP> KisExperimentPaintOpSettings::uniformProperties(KisPaintOpSettingsSP settings, PkPointer<KisPaintOpPresetUpdateProxy> updateProxy)
{
    PkList<KisUniformPaintOpPropertySP> props =
        listWeakToStrong(m_d->uniformProperties);

    if (props.isEmpty()) {
        {
            KisIntSliderBasedPaintOpPropertyCallback *prop =
                new KisIntSliderBasedPaintOpPropertyCallback(KisIntSliderBasedPaintOpPropertyCallback::Int, KoID("shape_speed", "Speed"), settings, 0);

            prop->setRange(0, 100);
            prop->setSingleStep(1);
            prop->setSuffix("%");

            prop->setReadCallback(
                [](KisUniformPaintOpProperty *prop) {
                    KisExperimentOpOptionData option;
                    option.read(prop->settings().data());

                    prop->setValue(int(option.speed));
                });
            prop->setWriteCallback(
                [](KisUniformPaintOpProperty *prop) {
                    KisExperimentOpOptionData option;
                    option.read(prop->settings().data());
                    option.speed = prop->value().toInt();
                    option.write(prop->settings().data());
                });
            prop->setIsVisibleCallback(
                [](const KisUniformPaintOpProperty *prop) -> bool {
                    KisExperimentOpOptionData option;
                    option.read(prop->settings().data());
                    return option.isSpeedEnabled;
                });

            PkObject::connect(updateProxy, &KisPaintOpPresetUpdateProxy::sigSettingsChanged, prop, &KisUniformPaintOpProperty::requestReadValue);
            prop->requestReadValue();
            props << toQShared(prop);
        }
        {
            KisIntSliderBasedPaintOpPropertyCallback *prop =
                new KisIntSliderBasedPaintOpPropertyCallback(KisIntSliderBasedPaintOpPropertyCallback::Int, KoID("shape_smooth", "Smooth"), settings, 0);

            prop->setRange(0, 100);
            prop->setSingleStep(1);
            prop->setSuffix(" px");

            prop->setReadCallback(
                [](KisUniformPaintOpProperty *prop) {
                    KisExperimentOpOptionData option;
                    option.read(prop->settings().data());

                    prop->setValue(int(option.smoothing));
                });
            prop->setWriteCallback(
                [](KisUniformPaintOpProperty *prop) {
                    KisExperimentOpOptionData option;
                    option.read(prop->settings().data());
                    option.smoothing = prop->value().toInt();
                    option.write(prop->settings().data());
                });
            prop->setIsVisibleCallback(
                [](const KisUniformPaintOpProperty *prop) {
                    KisExperimentOpOptionData option;
                    option.read(prop->settings().data());
                    return option.isSmoothingEnabled;
                });

            PkObject::connect(updateProxy, &KisPaintOpPresetUpdateProxy::sigSettingsChanged, prop, &KisUniformPaintOpProperty::requestReadValue);
            prop->requestReadValue();
            props << toQShared(prop);
        }

        {
            KisIntSliderBasedPaintOpPropertyCallback *prop = new KisIntSliderBasedPaintOpPropertyCallback(KisIntSliderBasedPaintOpPropertyCallback::Int,
                                                                                                          KoID("shape_displace", "Displace"),
                                                                                                          settings,
                                                                                                          0);

            prop->setRange(0, 100);
            prop->setSingleStep(1);
            prop->setSuffix("%");

            prop->setReadCallback(
                [](KisUniformPaintOpProperty *prop) {
                    KisExperimentOpOptionData option;
                    option.read(prop->settings().data());

                    prop->setValue(int(option.displacement));
                });
            prop->setWriteCallback(
                [](KisUniformPaintOpProperty *prop) {
                    KisExperimentOpOptionData option;
                    option.read(prop->settings().data());
                    option.displacement = prop->value().toInt();
                    option.write(prop->settings().data());
                });
            prop->setIsVisibleCallback(
                [](const KisUniformPaintOpProperty *prop) {
                    KisExperimentOpOptionData option;
                    option.read(prop->settings().data());
                    return option.isDisplacementEnabled;
                });

            PkObject::connect(updateProxy, &KisPaintOpPresetUpdateProxy::sigSettingsChanged, prop, &KisUniformPaintOpProperty::requestReadValue);
            prop->requestReadValue();
            props << toQShared(prop);
        }

        {
            KisUniformPaintOpPropertyCallback *prop =
                new KisUniformPaintOpPropertyCallback(KisUniformPaintOpPropertyCallback::Bool, KoID("shape_windingfill", "Winding Fill"), settings, 0);

            prop->setReadCallback(
                [](KisUniformPaintOpProperty *prop) {
                    KisExperimentOpOptionData option;
                    option.read(prop->settings().data());

                    prop->setValue(option.windingFill);
                });
            prop->setWriteCallback(
                [](KisUniformPaintOpProperty *prop) {
                    KisExperimentOpOptionData option;
                    option.read(prop->settings().data());
                    option.windingFill = prop->value().toBool();
                    option.write(prop->settings().data());
                });

            PkObject::connect(updateProxy, &KisPaintOpPresetUpdateProxy::sigSettingsChanged, prop, &KisUniformPaintOpProperty::requestReadValue);
            prop->requestReadValue();
            props << toQShared(prop);
        }

        {
            KisUniformPaintOpPropertyCallback *prop =
                new KisUniformPaintOpPropertyCallback(KisUniformPaintOpPropertyCallback::Bool, KoID("shape_hardedge", "Hard Edge"), settings, 0);

            prop->setReadCallback(
                [](KisUniformPaintOpProperty *prop) {
                    KisExperimentOpOptionData option;
                    option.read(prop->settings().data());

                    prop->setValue(option.hardEdge);
                });
            prop->setWriteCallback(
                [](KisUniformPaintOpProperty *prop) {
                    KisExperimentOpOptionData option;
                    option.read(prop->settings().data());
                    option.hardEdge = prop->value().toBool();
                    option.write(prop->settings().data());
                });

            PkObject::connect(updateProxy, &KisPaintOpPresetUpdateProxy::sigSettingsChanged, prop, &KisUniformPaintOpProperty::requestReadValue);
            prop->requestReadValue();
            props << toQShared(prop);
        }
    }

    {
        using namespace KisStandardUniformPropertiesFactory;

        Q_FOREACH (KisUniformPaintOpPropertySP prop, KisPaintOpSettings::uniformProperties(settings, updateProxy)) {
            if (prop->id() == opacity.id()) {
                props.prepend(prop);
            }
        }
    }

    return props;
}
