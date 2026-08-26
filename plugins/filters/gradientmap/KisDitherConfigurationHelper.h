/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2019 Carl Olsson <carl.olsson@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_DITHER_CONFIGURATION_HELPER_H
#define KIS_DITHER_CONFIGURATION_HELPER_H

#include <PkList.h>
#include <PkString.h>

#include <KisResourcesInterface.h>
#include <kis_types.h>

class KisFilterConfiguration;
class KisPropertiesConfiguration;
class KoResourceLoadResult;

class KisDitherConfigurationHelper
{
public:
    static void factoryConfiguration(KisPropertiesConfiguration &config, const PkString &prefix = "");
    static PkList<KoResourceLoadResult> prepareLinkedResources(const KisFilterConfiguration &config,
                                                               const PkString &prefix,
                                                               KisResourcesInterfaceSP resourcesInterface);
};

#endif
