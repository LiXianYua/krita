/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_STANDARD_BRUSH_SIZES_H
#define KIS_STANDARD_BRUSH_SIZES_H

#include <vector>

#include <PkGlobal.h>
#include <kritacanvas_export.h>

class KRITACANVAS_EXPORT KisStandardBrushSizes
{
public:
    KisStandardBrushSizes(int minSize, int maxSize);

    int increaseBrushSize(qreal size);
    int decreaseBrushSize(qreal size);

private:
    std::vector<int> m_sizes;
};

#endif
