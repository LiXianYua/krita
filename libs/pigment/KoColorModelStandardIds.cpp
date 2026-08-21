/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "KoColorModelStandardIds.h"

#include <PkString.h>


const KoID AlphaColorModelID("A", PkString("Alpha mask"));
const KoID RGBAColorModelID("RGBA", PkString("RGB/Alpha"));
const KoID XYZAColorModelID("XYZA", PkString("XYZ/Alpha"));
const KoID LABAColorModelID("LABA", PkString("L*a*b*/Alpha"));
const KoID CMYKAColorModelID("CMYKA", PkString("CMYK/Alpha"));
const KoID GrayAColorModelID("GRAYA", PkString("Grayscale/Alpha"));
const KoID GrayColorModelID("GRAY", PkString("Grayscale (without transparency)"));
const KoID YCbCrAColorModelID("YCbCrA", PkString("YCbCr/Alpha"));

const KoID Integer8BitsColorDepthID("U8", PkString("8-bit integer/channel"));
const KoID Integer16BitsColorDepthID("U16", PkString("16-bit integer/channel"));
const KoID Float16BitsColorDepthID("F16", PkString("16-bit float/channel"));
const KoID Float32BitsColorDepthID("F32", PkString("32-bit float/channel"));
const KoID Float64BitsColorDepthID("F64", PkString("64-bit float/channel"));
