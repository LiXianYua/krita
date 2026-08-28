/*
 * This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2020 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "multigridpatterngenerator.h"

#include <kis_paint_device.h>
#include <generator/kis_generator_registry.h>
#include <kis_global.h>
#include <kis_types.h>
#include <filter/kis_filter_configuration.h>
#include <kis_processing_information.h>
namespace { const bool multigridGeneratorRegistered = [] {
    KisGeneratorRegistry::instance()->add(new KisMultigridPatternGenerator());
    return true;
}(); }

KisMultigridPatternGenerator::KisMultigridPatternGenerator() : KisGenerator(id(), KoID("basic"), "&Multigrid Pattern...")
{
    setColorSpaceIndependence(FULLY_INDEPENDENT);
    setSupportsPainting(true);
}

KisFilterConfigurationSP KisMultigridPatternGenerator::defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const
{
    KisFilterConfigurationSP config = factoryConfiguration(resourcesInterface);

    config->setProperty("gradientXML", multigridDefaultGradientXml());

    PkVariant v;
    KoColor c;
    v.setValue(c);
    config->setProperty("lineColor", v);
    config->setProperty("divisions", 5);
    config->setProperty("lineWidth", 1);
    config->setProperty("dimensions", 5);
    config->setProperty("offset", .2);

    config->setProperty("colorRatio", 1.0);
    config->setProperty("colorIndex", 0.0);
    config->setProperty("colorIntersect", 0.0);

    config->setProperty("connectorColor", v);
    config->setProperty("connectorType", Connector::None);
    config->setProperty("connectorWidth", 1);
    return config;
}

void KisMultigridPatternGenerator::generate(KisProcessingInformation dstInfo,
                                 const PkSize& size,
                                 const KisFilterConfigurationSP config,
                                 KoUpdater* progressUpdater) const
{
    KisPaintDeviceSP dst = dstInfo.paintDevice();

    Q_ASSERT(!dst.isNull());
    Q_ASSERT(config);

    if (!config) {
        return;
    }

    const int divisions = config->getInt("divisions", 1);
    const int dimensions = config->getInt("dimensions", 5);
    const qreal offset = config->getFloat("offset", .2);
    const PkList<KisMultiGridRhomb> rhombs =
        generateMultigridRhombs(dimensions, divisions, offset);

    // S-09/M5 GAP: geometry generation remains live and deterministic; the
    // unavailable painter/gradient renderer deliberately does not mutate dst.
    (void)rhombs;
    (void)size;
    (void)progressUpdater;
}
