/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2006 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef COLOR_GENERATOR_H
#define COLOR_GENERATOR_H

#include <PkVariant.h>
#include "generator/kis_generator.h"


class KisColorGenerator : public KisGenerator
{
public:

    KisColorGenerator();

    using KisGenerator::generate;

    void generate(KisProcessingInformation dst,
                  const PkSize& size,
                  const KisFilterConfigurationSP config,
                  KoUpdater* progressUpdater
                 ) const override;

    static inline KoID id() {
        return KoID("color", "Color");
    }
    KisFilterConfigurationSP defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const override;
};

#endif
