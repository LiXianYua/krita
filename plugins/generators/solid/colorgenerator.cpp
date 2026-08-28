/*
 * This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2008 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "colorgenerator.h"

#include <PkPoint.h>

#include <kis_debug.h>


#include <kis_fill_painter.h>
#include <kis_image.h>
#include <kis_paint_device.h>
#include <kis_layer.h>
#include <generator/kis_generator_registry.h>
#include <kis_global.h>
#include <kis_selection.h>
#include <kis_types.h>
#include <filter/kis_filter_configuration.h>
#include <kis_processing_information.h>

namespace { const bool registered = [] {
    KisGeneratorRegistry::instance()->add(new KisColorGenerator());
    return true;
}(); }

KisColorGenerator::KisColorGenerator() : KisGenerator(id(), KoID("basic"), "&Solid Color...")
{
    setColorSpaceIndependence(FULLY_INDEPENDENT);
    setSupportsPainting(true);
}

KisFilterConfigurationSP KisColorGenerator::defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const
{
    KisFilterConfigurationSP config = factoryConfiguration(resourcesInterface);

    PkVariant v;
    v.setValue(KoColor());
    config->setProperty("color", v);
    return config;
}

void KisColorGenerator::generate(KisProcessingInformation dstInfo,
                                 const PkSize& size,
                                 const KisFilterConfigurationSP config,
                                 KoUpdater* progressUpdater) const
{
    KisPaintDeviceSP dst = dstInfo.paintDevice();

    Q_ASSERT(!dst.isNull());
    Q_ASSERT(config);

    KoColor c;
    if (config) {
        c = config->getColor("color");


        KisFillPainter gc(dst);
        gc.setProgress(progressUpdater);
        gc.setChannelFlags(config->channelFlags());
        gc.setOpacityF(0.4);
        gc.setSelection(dstInfo.selection());
        gc.fillRect(PkRect(dstInfo.topLeft(), size), c);
        gc.end();
    }
}
