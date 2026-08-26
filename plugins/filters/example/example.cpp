/*
 * This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2004 Cyrille Berger <cberger@cberger.net>
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "example.h"
#include <stdlib.h>
#include <vector>

#include <PkPoint.h>


#include <kis_debug.h>

#include <kis_processing_information.h>
#include <kis_types.h>
#include <kis_selection.h>
#include <kis_layer.h>
#include <filter/kis_filter_category_ids.h>
#include <filter/kis_filter_registry.h>
#include <kis_global.h>
#include "KoColorModelStandardIds.h"
#include "kis_filter_configuration.h"

namespace {
struct KritaExampleFilterRegistration
{
    KritaExampleFilterRegistration()
    {
        KisFilterRegistry::instance()->add(KisFilterSP(new KisFilterInvert()));
    }
};
} // namespace
static KritaExampleFilterRegistration s_kritaExampleFilterRegistration;


KisFilterInvert::KisFilterInvert() : KisColorTransformationFilter(id(), FiltersCategoryAdjustId, PkString("&Invert"))
{
    setColorSpaceIndependence(FULLY_INDEPENDENT);
    setSupportsPainting(true);
    setShowConfigurationWidget(false);
    setSupportsLevelOfDetail(true);
}

KoColorTransformation* KisFilterInvert::createTransformation(const KoColorSpace* cs, const KisFilterConfigurationSP config) const
{
    Q_UNUSED(config);
    return cs->createInvertTransformation();
}

bool KisFilterInvert::needsTransparentPixels(const KisFilterConfigurationSP config, const KoColorSpace *cs) const
{
    Q_UNUSED(config);
    return cs->colorModelId() == AlphaColorModelID;
}
