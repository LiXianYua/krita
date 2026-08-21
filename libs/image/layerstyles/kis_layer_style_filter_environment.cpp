/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// ===========================================================================
// [GAP] kis_layer_style_filter_environment.cpp 阻塞登记（S-06 Task 6，修复轮更新）
//
// 本文件不进薄壳，仅剥可机械映射类型（源文件 Q* 已归零）。原「协变返回断裂」
// 阻塞点已由 KoResource 族 PkVector→PkList 修复解除（修复轮验证：编过）。现阻塞：
//   * mid()：include krita_utils.h 的模板 rasterizePolygonDDA 用 Qt 序列容器
//     mid()，PkVector 无此方法，定义期即报错（krita_utils.h:270）。
// 关闭条件：给 PkVector 补 mid()，或 krita_utils.h 该模板改用 Pk 容器接口。
// 当前状态：Qt 仅经未剥依赖头传递进入，不参与薄壳构建。
// ===========================================================================


#include "kis_layer_style_filter_environment.h"

#include <PkBitArray.h>

#include "kis_layer.h"
#include "kis_ls_utils.h"

#include "kis_selection.h"
#include "kis_pixel_selection.h"
#include "kis_painter.h"
#include "kis_image.h"

#include "krita_utils.h"

#include <boost/random/mersenne_twister.hpp>
#include "kis_random_accessor_ng.h"
#include "kis_iterator_ng.h"
#include "kis_cached_paint_device.h"
#include "KisLocalStrokeResources.h"


struct Q_DECL_HIDDEN KisLayerStyleFilterEnvironment::Private
{
    KisLayer *sourceLayer;
    KisPixelSelectionSP cachedRandomSelection;
    KisCachedSelection globalCachedSelection;
    KisCachedPaintDevice globalCachedPaintDevice;
    KisLocalStrokeResources cachedFlattenedPattern;

    static KisPixelSelectionSP generateRandomSelection(const PkRect &rc);
};


KisPixelSelectionSP
KisLayerStyleFilterEnvironment::Private::
generateRandomSelection(const PkRect &rc)
{
    KisPixelSelectionSP selection = new KisPixelSelection();
    KisSequentialIterator dstIt(selection, rc);

    boost::mt11213b uniformSource;

    if (uniformSource.max() >= 0x00FFFFFF) {
        while (dstIt.nextPixel()) {
            int randValue = uniformSource();
            *dstIt.rawData() = (quint8) randValue;

            if (!dstIt.nextPixel()) break;
            randValue >>= 8;
            *dstIt.rawData() = (quint8) randValue;

            if (!dstIt.nextPixel()) break;
            randValue >>= 8;
            *dstIt.rawData() = (quint8) randValue;
        }

    } else {
        while (dstIt.nextPixel()) {
            *dstIt.rawData() = (quint8) uniformSource();
        }
    }

    return selection;
}

KisLayerStyleFilterEnvironment::KisLayerStyleFilterEnvironment(KisLayer *sourceLayer)
    : m_d(new Private)
{
    Q_ASSERT(sourceLayer);
    m_d->sourceLayer = sourceLayer;
}

KisLayerStyleFilterEnvironment::~KisLayerStyleFilterEnvironment()
{
}

PkRect KisLayerStyleFilterEnvironment::layerBounds() const
{
    return m_d->sourceLayer ? m_d->sourceLayer->projection()->exactBounds() : PkRect(0, 0, 0, 0);
}

PkRect KisLayerStyleFilterEnvironment::defaultBounds() const
{
    return m_d->sourceLayer ?
        m_d->sourceLayer->original()->defaultBounds()->bounds() : PkRect(0, 0, 0, 0);
}

int KisLayerStyleFilterEnvironment::currentLevelOfDetail() const
{
    return m_d->sourceLayer ?
        m_d->sourceLayer->original()->defaultBounds()->currentLevelOfDetail() : 0;
}

void KisLayerStyleFilterEnvironment::setupFinalPainter(KisPainter *gc,
                                                       quint8 opacity,
                                                       const PkBitArray &channelFlags) const
{
    Q_ASSERT(m_d->sourceLayer);
    gc->setOpacityF(KritaUtils::mergeOpacityF(qreal(opacity) / OPACITY_OPAQUE_U8, qreal(m_d->sourceLayer->opacity()) / OPACITY_OPAQUE_U8));
    gc->setChannelFlags(KritaUtils::mergeChannelFlags(channelFlags, m_d->sourceLayer->channelFlags()));

}

KisPixelSelectionSP KisLayerStyleFilterEnvironment::cachedRandomSelection(const PkRect &requestedRect) const
{
    KisPixelSelectionSP selection = m_d->cachedRandomSelection;

    PkRect existingRect;

    if (selection) {
        existingRect = selection->selectedExactRect();
    }

    if (!existingRect.contains(requestedRect)) {
        m_d->cachedRandomSelection =
            Private::generateRandomSelection(requestedRect | existingRect);
    }

    return m_d->cachedRandomSelection;
}

KoPatternSP KisLayerStyleFilterEnvironment::cachedFlattenedPattern(KoPatternSP pattern) const
{
    if (!pattern->hasAlpha()) return pattern;

    auto source = m_d->cachedFlattenedPattern.source<KoPattern>(ResourceType::Patterns);

    KoPatternSP resultPattern = source.bestMatch("", pattern->filename(), pattern->name());
    if (resultPattern) return resultPattern;

    KoPatternSP flattenedPattern = pattern->cloneWithoutAlpha();

    m_d->cachedFlattenedPattern.addResource(flattenedPattern);

    return flattenedPattern;
}

KisCachedSelection *KisLayerStyleFilterEnvironment::cachedSelection()
{
    return &m_d->globalCachedSelection;
}

KisCachedPaintDevice *KisLayerStyleFilterEnvironment::cachedPaintDevice()
{
    return &m_d->globalCachedPaintDevice;
}
