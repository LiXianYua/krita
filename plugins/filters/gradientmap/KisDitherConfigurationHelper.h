/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2019 Carl Olsson <carl.olsson@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_DITHER_CONFIGURATION_HELPER_H
#define KIS_DITHER_CONFIGURATION_HELPER_H

#include <QList>
#include <QString>

#include <KisResourcesInterface.h>
#include <kis_types.h>

class KisFilterConfiguration;
class KisPropertiesConfiguration;
class KoResourceLoadResult;

class KisDitherConfigurationHelper
{
public:
    static void factoryConfiguration(KisPropertiesConfiguration &config, const QString &prefix = "");
    static QList<KoResourceLoadResult> prepareLinkedResources(const KisFilterConfiguration &config,
                                                               const QString &prefix,
                                                               KisResourcesInterfaceSP resourcesInterface);
};

#endif
