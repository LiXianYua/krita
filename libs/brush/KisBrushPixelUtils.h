/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_BRUSH_PIXEL_UTILS_H
#define KIS_BRUSH_PIXEL_UTILS_H

#include <PkRgb.h>

inline int kisBrushGray(PkRgb rgb)
{
    return (pkRed(rgb) * 11 + pkGreen(rgb) * 16 + pkBlue(rgb) * 5) / 32;
}

#endif
