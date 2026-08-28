#include <PkString.h>
/*
 *  SPDX-FileCopyrightText: 2010 Lukáš Tvrdý <lukast.dev@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KisHairyBristleOptionData.h"

#include "kis_properties_configuration.h"
#include <kis_paintop_lod_limitations.h>


const PkString HAIRY_BRISTLE_USE_MOUSEPRESSURE = "HairyBristle/useMousePressure";
const PkString HAIRY_BRISTLE_SCALE = "HairyBristle/scale";
const PkString HAIRY_BRISTLE_SHEAR = "HairyBristle/shear";
const PkString HAIRY_BRISTLE_RANDOM = "HairyBristle/random";
const PkString HAIRY_BRISTLE_DENSITY = "HairyBristle/density";
const PkString HAIRY_BRISTLE_THRESHOLD = "HairyBristle/threshold";
const PkString HAIRY_BRISTLE_ANTI_ALIASING = "HairyBristle/antialias";
const PkString HAIRY_BRISTLE_USE_COMPOSITING = "HairyBristle/useCompositing";
const PkString HAIRY_BRISTLE_CONNECTED = "HairyBristle/isConnected";


bool KisHairyBristleOptionData::read(const KisPropertiesConfiguration *setting)
{
    useMousePressure = setting->getBool(HAIRY_BRISTLE_USE_MOUSEPRESSURE, false);
    shearFactor = setting->getDouble(HAIRY_BRISTLE_SHEAR, 0.0);
    randomFactor = setting->getDouble(HAIRY_BRISTLE_RANDOM, 2.0);
    scaleFactor = setting->getDouble(HAIRY_BRISTLE_SCALE, 2.0);
    densityFactor = setting->getDouble(HAIRY_BRISTLE_DENSITY, 100.0);
    threshold = setting->getBool(HAIRY_BRISTLE_THRESHOLD, false);
    antialias = setting->getBool(HAIRY_BRISTLE_ANTI_ALIASING, false);
    useCompositing = setting->getBool(HAIRY_BRISTLE_USE_COMPOSITING, false);
    connectedPath = setting->getBool(HAIRY_BRISTLE_CONNECTED, false);

    return true;
}

void KisHairyBristleOptionData::write(KisPropertiesConfiguration *setting) const
{
    setting->setProperty(HAIRY_BRISTLE_USE_MOUSEPRESSURE, useMousePressure);
    setting->setProperty(HAIRY_BRISTLE_SHEAR, shearFactor);
    setting->setProperty(HAIRY_BRISTLE_RANDOM, randomFactor);
    setting->setProperty(HAIRY_BRISTLE_SCALE, scaleFactor);
    setting->setProperty(HAIRY_BRISTLE_DENSITY, densityFactor);
    setting->setProperty(HAIRY_BRISTLE_THRESHOLD, threshold);
    setting->setProperty(HAIRY_BRISTLE_ANTI_ALIASING, antialias);
    setting->setProperty(HAIRY_BRISTLE_USE_COMPOSITING, useCompositing);
    setting->setProperty(HAIRY_BRISTLE_CONNECTED, connectedPath);
}

KisPaintopLodLimitations KisHairyBristleOptionData::lodLimitations() const
{
    KisPaintopLodLimitations l;
    l.limitations.insert(KoID("hairy-brush", "Bristle Brush (the lines will be thinner than on preview)"));
    return l;
}
