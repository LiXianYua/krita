/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2020 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <boost/none.hpp>
#include <PkObject.h>
#include <PkSharedPointer.h>
#include <kis_generator.h>
#include <kis_generator_layer.h>
#include <KisRunnableBasedStrokeStrategy.h>

class KisGeneratorStrokeStrategy: public PkObject, public KisRunnableBasedStrokeStrategy
{
public:
    KisGeneratorStrokeStrategy();
    ~KisGeneratorStrokeStrategy() override;

    static PkVector<KisStrokeJobData *> createJobsData(const KisGeneratorLayerSP layer, PkSharedPointer<boost::none_t> cookie, const KisGeneratorSP f, const KisPaintDeviceSP dev, const PkRegion &rc, const KisFilterConfigurationSP filterConfig);
};
