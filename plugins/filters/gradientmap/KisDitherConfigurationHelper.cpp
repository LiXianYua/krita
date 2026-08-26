/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2019 Carl Olsson <carl.olsson@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisDitherConfigurationHelper.h"

#include <KoPattern.h>
#include <KoResourceLoadResult.h>
#include <KisResourcesInterface.h>
#include <kis_filter_configuration.h>
#include <kis_properties_configuration.h>

#include "KisDitherUtil.h"

void KisDitherConfigurationHelper::factoryConfiguration(KisPropertiesConfiguration &config, const PkString &prefix)
{
    config.setProperty(prefix + "thresholdMode", KisDitherUtil::ThresholdMode::Pattern);
    config.setProperty(prefix + "pattern", "DITH 0202 GEN ");
    config.setProperty(prefix + "patternValueMode", KisDitherUtil::PatternValueMode::Auto);
    config.setProperty(prefix + "noiseSeed", rand());
    config.setProperty(prefix + "spread", 1.0);
}

PkList<KoResourceLoadResult> KisDitherConfigurationHelper::prepareLinkedResources(const KisFilterConfiguration &config,
                                                                                  const PkString &prefix,
                                                                                  KisResourcesInterfaceSP resourcesInterface)
{
    auto source = resourcesInterface->source<KoPattern>(ResourceType::Patterns);

    PkString patternMD5 = config.getString(prefix + "md5sum");
    PkString patternName = config.getString(prefix + "pattern");

    return {source.bestMatchLoadResult(patternMD5, "", patternName)};
}
