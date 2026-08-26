/*
 * SPDX-FileCopyrightText: 2014 Manuel Riecke <spell1337@gmail.com>
 *
 * SPDX-License-Identifier: ICS
 */

#pragma once

#include <PkAuxTypes.h>
#include <PkColor.h>
#include "indexcolorpalette.h"

struct PaletteGeneratorConfig
{
    PkColor colors[4][4];
    bool   colorsEnabled[4][4];
    int    gradientSteps[3];
    int    inbetweenRampSteps;
    bool   diagonalGradients;

    PaletteGeneratorConfig();
    PkByteArray toByteArray();
    void fromByteArray(const PkByteArray& str);
    IndexColorPalette generate();
};
