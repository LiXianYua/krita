/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2006 Frederic Coiffier <fcoiffie@gmail.com>
 * SPDX-FileCopyrightText: 2021 Deif Lou <ginoba@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <filter/kis_filter_category_ids.h>

#include "../colorsfilters/kis_multichannel_utils.h"

#include "KisLevelsFilter.h"

KisLevelsFilter::KisLevelsFilter()
    : KisColorTransformationFilter(id(), FiltersCategoryAdjustId, i18n("&Levels..."))
{
    setSupportsPainting(true);
    setColorSpaceIndependence(TO_LAB16);
    setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
}

KisFilterConfigurationSP KisLevelsFilter::factoryConfiguration(KisResourcesInterfaceSP resourcesInterface) const
{
    return new KisLevelsFilterConfiguration(0, resourcesInterface);
}

KoColorTransformation* KisLevelsFilter::createTransformation(const KoColorSpace* cs, const KisFilterConfigurationSP config) const
{
    Q_ASSERT(cs);
    const KisLevelsFilterConfiguration *config_ = dynamic_cast<const KisLevelsFilterConfiguration*>(config.data());
    Q_ASSERT(config_);

    if (config_->useLightnessMode()) {
        if (!config_->lightnessLevelsCurve().isIdentity()) {
            return cs->createBrightnessContrastAdjustment(config_->lightnessTransfer().constData());
        }
    } else {
        PkList<bool> isIdentityList;
        for (const KisLevelsCurve &levelsCurve : config_->levelsCurves()) {
            isIdentityList.append(levelsCurve.isIdentity());
        }

        return KisMultiChannelUtils::createPerChannelTransformationFromTransfers(cs, config_->transfers(), isIdentityList);
    }
    return nullptr;
}
