/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_COLOR_SAMPLING_UTILS_H
#define KIS_COLOR_SAMPLING_UTILS_H

#include <kis_types.h>
#include <PkPoint.h>
#include <kritaimage_export.h>

class KoColor;
class PkPoint;

namespace KisColorSamplingUtils
{
KRITAIMAGE_EXPORT bool sampleColor(KoColor &outColor,
                                   KisPaintDeviceSP device,
                                   const PkPoint &position,
                                   const KoColor *blendColor = nullptr,
                                   int radius = 1,
                                   int blend = 100,
                                   bool pure = false);
}

#endif
