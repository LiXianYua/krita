/*
 *  SPDX-FileCopyrightText: 2022 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KisFilterOptionData.h"

#include <kis_paintop_settings.h>
#include <kis_properties_configuration.h>

const PkString FILTER_ID = "Filter/id";
const PkString FILTER_SMUDGE_MODE = "Filter/smudgeMode";
const PkString FILTER_CONFIGURATION = "Filter/configuration";

bool KisFilterOptionData::read(const KisPropertiesConfiguration *setting)
{
    filterId = setting->getString(FILTER_ID);
    filterConfig = setting->getString(FILTER_CONFIGURATION);
    smudgeMode = setting->getBool(FILTER_SMUDGE_MODE);

    return true;
}

void KisFilterOptionData::write(KisPropertiesConfiguration *setting) const
{
    setting->setProperty(FILTER_ID, filterId);
    setting->setProperty(FILTER_CONFIGURATION, filterConfig);
    setting->setProperty(FILTER_SMUDGE_MODE, smudgeMode);
}

PkString KisFilterOptionData::filterIdTag()
{
    return FILTER_ID;
}

PkString KisFilterOptionData::filterConfigTag()
{
    return FILTER_CONFIGURATION;
}

