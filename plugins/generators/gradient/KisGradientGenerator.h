/*
 * KDE. Krita Project.
 *
 * SPDX-FileCopyrightText: 2020 Deif Lou <ginoba@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISGRADIENTGENERATOR_H
#define KISGRADIENTGENERATOR_H

#include "generator/kis_generator.h"

#include "KisGradientGeneratorConfiguration.h"


class KisGradientGenerator : public KisGenerator
{
public:
    KisGradientGenerator();

    using KisGenerator::generate;

    virtual void generate(KisProcessingInformation dst,
                          const PkSize& size,
                          const KisFilterConfigurationSP config,
                          KoUpdater* progressUpdater) const override;
    
    static inline KoID id() {
        return KoID(KisGradientGeneratorConfiguration::defaultName(), "Gradient");
    }

    KisFilterConfigurationSP factoryConfiguration(KisResourcesInterfaceSP resourcesInterface) const override;
    KisFilterConfigurationSP defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const override;
};

#endif
