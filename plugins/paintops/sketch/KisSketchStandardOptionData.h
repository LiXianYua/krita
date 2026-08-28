/*
 *  SPDX-FileCopyrightText: 2011 Lukáš Tvrdý <lukast.dev@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KISSKETCHSTANDARDOPTIONDATA_H
#define KISSKETCHSTANDARDOPTIONDATA_H

#include <KisCurveOptionData.h>

struct KisOffsetScaleOptionData : KisCurveOptionData
{
    KisOffsetScaleOptionData()
        : KisCurveOptionData(
              KoID("Offset scale", "Offset scale"))
    {}
};

struct KisLineWidthOptionData : KisCurveOptionData
{
    KisLineWidthOptionData()
        : KisCurveOptionData(
              KoID("Line width", "Line width"))
    {}
};

struct KisDensityOptionData : KisCurveOptionData
{
    KisDensityOptionData()
        : KisCurveOptionData(
              KoID("Density", "Density"))
    {}
};

#endif // KISSKETCHSTANDARDOPTIONDATA_H
