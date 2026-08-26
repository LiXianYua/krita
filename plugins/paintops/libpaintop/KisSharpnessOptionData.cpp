/*
 *  SPDX-FileCopyrightText: 2022 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KisSharpnessOptionData.h"

#include <kis_properties_configuration.h>

const PkString SHARPNESS_FACTOR = "Sharpness/factor";
const PkString SHARPNESS_ALIGN_OUTLINE_PIXELS = "Sharpness/alignoutline";
const PkString SHARPNESS_SOFTNESS  = "Sharpness/softness";


bool KisSharpnessOptionMixInImpl::read(const KisPropertiesConfiguration *setting)
{
    alignOutlinePixels = setting->getBool(SHARPNESS_ALIGN_OUTLINE_PIXELS);
    softness = setting->getInt(SHARPNESS_SOFTNESS);

    if (setting->hasProperty(SHARPNESS_FACTOR) && !setting->hasProperty("SharpnessValue")) {
        softness = quint32(setting->getDouble(SHARPNESS_FACTOR) * 100);
    }

    return true;
}

void KisSharpnessOptionMixInImpl::write(KisPropertiesConfiguration *setting) const
{
    setting->setProperty(SHARPNESS_ALIGN_OUTLINE_PIXELS, alignOutlinePixels);
    setting->setProperty(SHARPNESS_SOFTNESS, softness);
}

KisSharpnessOptionData::KisSharpnessOptionData(const PkString &prefix)
    : KisOptionTuple<KisCurveOptionData, KisSharpnessOptionMixIn>(prefix, KoID("Sharpness", i18n("Sharpness")))
{
    valueFixUpReadCallback = [] (KisCurveOptionDataCommon *data, const KisPropertiesConfiguration *setting) {

        if (setting->hasProperty(SHARPNESS_FACTOR) && !setting->hasProperty("SharpnessValue")) {
            data->strengthValue = setting->getDouble(SHARPNESS_FACTOR);
        }
    };
}
