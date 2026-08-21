/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_LAYER_STYLE_FILTER_ENVIRONMENT_H
#define __KIS_LAYER_STYLE_FILTER_ENVIRONMENT_H

#include <PkScopedPointer.h>
#include <PkRect.h>

#include <kritaimage_export.h>
#include "kis_types.h"
#include <KoPattern.h>

class KisPainter;
class KisLayer;
class PkPainterPath;
class PkBitArray;
class KisCachedPaintDevice;
class KisCachedSelection;


class KRITAIMAGE_EXPORT KisLayerStyleFilterEnvironment
{
public:
    KisLayerStyleFilterEnvironment(KisLayer *sourceLayer);
    ~KisLayerStyleFilterEnvironment();

    PkRect layerBounds() const;
    PkRect defaultBounds() const;
    int currentLevelOfDetail() const;

    void setupFinalPainter(KisPainter *gc,
                           quint8 opacity,
                           const PkBitArray &channelFlags) const;

    KisPixelSelectionSP cachedRandomSelection(const PkRect &requestedRect) const;

    KoPatternSP cachedFlattenedPattern(KoPatternSP pattern) const;

    KisCachedSelection* cachedSelection();
    KisCachedPaintDevice* cachedPaintDevice();

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif /* __KIS_LAYER_STYLE_FILTER_ENVIRONMENT_H */
