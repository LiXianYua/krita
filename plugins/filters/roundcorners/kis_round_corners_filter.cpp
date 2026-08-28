/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2005 Michael Thaler <michael.thaler@physik.tu-muenchen.de>
 *
 * ported from Gimp, SPDX-FileCopyrightText: 1997 Eiichi Takamori <taka@ma1.seikyou.ne.jp>
 * original pixelize.c for GIMP 0.54 by Tracy Scott
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_round_corners_filter.h"

#include <stdlib.h>
#include <vector>
#include <math.h>

#include <PkPoint.h>


#include <KoUpdater.h>

#include <kis_debug.h>
#include <filter/kis_filter_registry.h>
#include <kis_global.h>
#include <kis_image.h>
#include <kis_layer.h>
#include <kis_selection.h>
#include <filter/kis_filter_category_ids.h>
#include <filter/kis_filter_configuration.h>
#include <kis_processing_information.h>
#include <kis_types.h>
#include <KisSequentialIteratorProgress.h>
#include <kis_algebra_2d.h>
#include <KoProgressUpdater.h>


KisRoundCornersFilter::KisRoundCornersFilter() : KisFilter(id(), FiltersCategoryMapId, PkString("&Round Corners..."))
{
    setSupportsPainting(false);

}

void fadeOneCorner(KisPaintDeviceSP device,
                   const PkPoint &basePoint,
                   const PkRect &processRect,
                   const qreal thresholdSq,
                   KoUpdater* progressUpdater)
{
    const KoColorSpace *cs = device->colorSpace();
    KisSequentialIteratorProgress dstIt(device, processRect, progressUpdater);

    while (dstIt.nextPixel()) {
        const PkPointF point(dstIt.x(), dstIt.y());

        const qreal distanceSq = kisSquareDistance(point, basePoint);
        if (distanceSq >= thresholdSq) {
            cs->setOpacity(dstIt.rawData(), OPACITY_TRANSPARENT_U8, 1);
        }
    }
}


void KisRoundCornersFilter::processImpl(KisPaintDeviceSP device,
                                        const PkRect& applyRect,
                                        const KisFilterConfigurationSP config,
                                        KoUpdater* progressUpdater
                                        ) const
{
    Q_UNUSED(config);
    Q_ASSERT(!device.isNull());

    if (!device || !config) {
        warnKrita << "Invalid parameters for round corner filter";
        dbgPlugins << device << " " << config;
        return;
    }

    const PkRect bounds = device->defaultBounds()->imageBorderRect();

    const qint32 radius = qMin(KisAlgebra2D::minDimension(bounds) / 2, qMax(1, config->getInt("radius" , 30)));
    const qreal radiusSq = pow2(radius);

    struct CornerJob {
        PkRect rc;
        PkPoint pt;
        KoUpdater *progressUpdater;
    };

    PkVector<CornerJob> jobs;

    KoProgressUpdater compositeUpdater(progressUpdater, KoProgressUpdater::Unthreaded);

    {
        PkRect rc(bounds.x(), bounds.y(), radius, radius);
        PkPoint pt(rc.bottomRight());
        jobs << CornerJob({rc, pt, compositeUpdater.startSubtask()});
    }

    {
        PkRect rc(bounds.x() + bounds.width() - radius, bounds.y(), radius, radius);
        PkPoint pt(rc.bottomLeft());
        jobs << CornerJob({rc, pt, compositeUpdater.startSubtask()});
    }

    {
        PkRect rc(bounds.x(), bounds.y() + bounds.height() - radius, radius, radius);
        PkPoint pt(rc.topRight());
        jobs << CornerJob({rc, pt, compositeUpdater.startSubtask()});
    }

    {
        PkRect rc(bounds.x() + bounds.width() - radius, bounds.y() + bounds.height() - radius, radius, radius);
        PkPoint pt(rc.topLeft());
        jobs << CornerJob({rc, pt, compositeUpdater.startSubtask()});
    }

    for (const CornerJob &job : jobs) {
        const PkRect processRect = job.rc & applyRect;
        if (!processRect.isEmpty()) {
            fadeOneCorner(device, job.pt, processRect, radiusSq, job.progressUpdater);
        }
    }
}

KisFilterConfigurationSP KisRoundCornersFilter::defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const
{
    KisFilterConfigurationSP config = factoryConfiguration(resourcesInterface);
    config->setProperty("radius", 30);
    return config;
}
