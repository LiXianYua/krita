/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// ===========================================================================
// [GAP] kis_ls_drop_shadow_filter.cpp 阻塞登记（S-06 Task 6）
//
// 本文件不进薄壳，仅剥可机械映射类型（源文件 Q* 已归零）。阻塞原因：
//   * 本文件传递 include 未剥依赖头，最终到达 kis_psd_layer_style.h（未剥），
//     其 KisPSDLayerStyle 用 Qt 列表容器覆盖 KoResource 的虚函数
//     （linkedResources/sideLoadedResources/requiredCanvasResources），
//     与 KoResource.h 被剥成的 PkVector 返回类型不一致 → 协变返回类型不匹配
// 关闭条件：KoResource.h 这 6 处签名是 旧列表容器→PkVector 的误映射（原始为 Qt 的列表容器类型，
// 按 Qt替代品选型 §1 应为 PkList；与 Task 3 修 KisRequiredResourcesOperators.h 同
// 一类缺陷）。KoResource.h + 各 override 统一改回 PkList 后本文件即编过。
// 当前状态：Qt 仅经未剥依赖头传递进入，不参与薄壳构建。
// ===========================================================================


#include "kis_ls_drop_shadow_filter.h"

#include <cstdlib>


#include <KoUpdater.h>
#include <resources/KoAbstractGradient.h>

#include "psd.h"

#include "kis_convolution_kernel.h"
#include "kis_convolution_painter.h"
#include "kis_gaussian_kernel.h"

#include "kis_pixel_selection.h"
#include "kis_fill_painter.h"
#include "kis_iterator_ng.h"
#include "kis_random_accessor_ng.h"

#include "kis_psd_layer_style.h"

#include "kis_multiple_projection.h"
#include "kis_ls_utils.h"
#include "kis_layer_style_filter_environment.h"
#include "kis_cached_paint_device.h"



KisLsDropShadowFilter::KisLsDropShadowFilter(Mode mode)
    : KisLayerStyleFilter(KoID("lsdropshadow", PkString("Drop Shadow (style)")))
    , m_mode(mode)
{
}

KisLsDropShadowFilter::KisLsDropShadowFilter(const KisLsDropShadowFilter &rhs)
    : KisLayerStyleFilter(rhs),
      m_mode(rhs.m_mode)
{
}

KisLayerStyleFilter *KisLsDropShadowFilter::clone() const
{
    return new KisLsDropShadowFilter(*this);
}

struct ShadowRectsData
{
    enum Direction {
        NEED_RECT,
        CHANGE_RECT
    };

    ShadowRectsData(const PkRect &applyRect,
                    const psd_layer_effects_context *context,
                    const psd_layer_effects_shadow_base *shadow,
                    Direction direction)
    {
        spread_size = (shadow->spread() * shadow->size() + 50) / 100;
        blur_size = shadow->size() - spread_size;
        offset = shadow->calculateOffset(context);

        // need rect calculation in reverse order
        dstRect = applyRect;

        const int directionCoeff = direction == NEED_RECT ? -1 : 1;
        srcRect = dstRect.translated(directionCoeff * offset);

        noiseNeedRect = shadow->noise() > 0 ?
            kisGrowRect(srcRect, KisLsUtils::noiseNeedBorder) : srcRect;

        blurNeedRect = blur_size ?
            KisLsUtils::growRectFromRadius(noiseNeedRect, blur_size) : noiseNeedRect;

        spreadNeedRect = spread_size ?
            KisLsUtils::growRectFromRadius(blurNeedRect, spread_size) : blurNeedRect;

        // dbgKrita << ppVar(dstRect);
        // dbgKrita << ppVar(srcRect);
        // dbgKrita << ppVar(noiseNeedRect);
        // dbgKrita << ppVar(blurNeedRect);
        // dbgKrita << ppVar(spreadNeedRect);
    }

    inline PkRect finalNeedRect() const {
        return spreadNeedRect;
    }

    inline PkRect finalChangeRect() const {
        // TODO: is it correct?
        return spreadNeedRect;
    }

    qint32 spread_size;
    qint32 blur_size;
    PkPoint offset;

    PkRect srcRect;
    PkRect dstRect;
    PkRect noiseNeedRect;
    PkRect blurNeedRect;
    PkRect spreadNeedRect;
};

void KisLsDropShadowFilter::applyDropShadow(KisPaintDeviceSP srcDevice,
                                            KisMultipleProjection *dst,
                                            const PkRect &applyRect,
                                            const psd_layer_effects_context *context,
                                            const psd_layer_effects_shadow_base *shadow,
                                            KisResourcesInterfaceSP resourcesInterface,
                                            KisLayerStyleFilterEnvironment *env) const
{
    if (applyRect.isEmpty()) return;

    ShadowRectsData d(applyRect, context, shadow, ShadowRectsData::NEED_RECT);

    KisCachedSelection::Guard s1(*env->cachedSelection());
    KisSelectionSP baseSelection = s1.selection();
    KisLsUtils::selectionFromAlphaChannel(srcDevice, baseSelection, d.spreadNeedRect);

    KisPixelSelectionSP selection = baseSelection->pixelSelection();

    //selection->convertToQImage(0, PkRect(0,0,300,300)).save("0_selection_initial.png");

    if (shadow->invertsSelection()) {
        selection->invert();
    }

    /**
     * Copy selection which will be erased from the original later
     */
    KisCachedSelection::Guard s2(*env->cachedSelection());
    KisPixelSelectionSP knockOutSelection;
    if (shadow->knocksOut()) {
        knockOutSelection = s2.selection()->pixelSelection();
        knockOutSelection->makeCloneFromRough(selection, selection->selectedRect());
    }

    if (shadow->technique() == psd_technique_precise) {
        KisLsUtils::findEdge(selection, d.blurNeedRect, true);
    }

    /**
     * Spread and blur the selection
     */
    if (d.spread_size) {
        KisLsUtils::applyGaussianWithTransaction(selection, d.blurNeedRect, d.spread_size);

        // TODO: find out why in libpsd we pass false here. If we do so,
        //       the result is fully black, which is not expected
        KisLsUtils::findEdge(selection, d.blurNeedRect, true /*shadow->edgeHidden()*/);
    }

    //selection->convertToQImage(0, PkRect(0,0,300,300)).save("1_selection_spread.png");

    if (d.blur_size) {
        KisLsUtils::applyGaussianWithTransaction(selection, d.noiseNeedRect, d.blur_size);
    }
    //selection->convertToQImage(0, PkRect(0,0,300,300)).save("2_selection_blur.png");

    if (shadow->range() != KisLsUtils::FULL_PERCENT_RANGE) {
        KisLsUtils::adjustRange(selection, d.noiseNeedRect, shadow->range());
    }

    const psd_layer_effects_inner_glow *iglow = 0;
    if ((iglow =
         dynamic_cast<const psd_layer_effects_inner_glow *>(shadow)) &&
        iglow->source() == psd_glow_center) {

        selection->invert();
    }

    /**
     * Contour correction
     */
    KisLsUtils::applyContourCorrection(selection,
                                       d.noiseNeedRect,
                                       shadow->contourLookupTable(),
                                       shadow->antiAliased(),
                                       shadow->edgeHidden());

    //selection->convertToQImage(0, PkRect(0,0,300,300)).save("3_selection_contour.png");

    /**
     * Noise
     */
    if (shadow->noise() > 0) {
        KisLsUtils::applyNoise(selection,
                               d.srcRect,
                               shadow->noise(),
                               context,
                               env);
    }
    //selection->convertToQImage(0, PkRect(0,0,300,300)).save("4_selection_noise.png");

    if (!d.offset.isNull()) {
        selection->moveTo(selection->offset() + d.offset);
    }

    /**
     * Knock-out original outline of the device from the resulting shade
     */
    if (shadow->knocksOut()) {
        KIS_ASSERT_RECOVER_RETURN(knockOutSelection);

        PkRect knockOutRect = !shadow->invertsSelection() ?
            d.srcRect : d.spreadNeedRect;

        knockOutRect &= d.dstRect;

        KisPainter gc(selection);
        gc.setCompositeOpId(COMPOSITE_ERASE);
        gc.bitBlt(knockOutRect.topLeft(), knockOutSelection, knockOutRect);
    }
    //selection->convertToQImage(0, PkRect(0,0,300,300)).save("5_selection_knockout.png");

    KisLsUtils::applyFinalSelection(KisMultipleProjection::defaultProjectionId(),
                                    baseSelection,
                                    srcDevice,
                                    dst,
                                    d.srcRect,
                                    d.dstRect,
                                    context,
                                    shadow,
                                    resourcesInterface,
                                    env);
}

const psd_layer_effects_shadow_base*
KisLsDropShadowFilter::getShadowStruct(KisPSDLayerStyleSP style) const
{
    const psd_layer_effects_shadow_base *config = 0;

    if (m_mode == DropShadow) {
        config = style->dropShadow();
    } else if (m_mode == InnerShadow) {
        config = style->innerShadow();
    } else if (m_mode == OuterGlow) {
        config = style->outerGlow();
    } else if (m_mode == InnerGlow) {
        config = style->innerGlow();
    }

    return config;
}

void KisLsDropShadowFilter::processDirectly(KisPaintDeviceSP src,
                                            KisMultipleProjection *dst,
                                            KisLayerStyleKnockoutBlower *blower,
                                            const PkRect &applyRect,
                                            KisPSDLayerStyleSP style,
                                            KisLayerStyleFilterEnvironment *env) const
{
    Q_UNUSED(blower);
    KIS_ASSERT_RECOVER_RETURN(style);

    const psd_layer_effects_shadow_base *config = getShadowStruct(style);
    if (!KisLsUtils::checkEffectEnabled(config, dst)) return;

    KisLsUtils::LodWrapper<psd_layer_effects_shadow_base> w(env->currentLevelOfDetail(), config);
    applyDropShadow(src, dst, applyRect, style->context(), w.config, style->resourcesInterface(), env);
}

PkRect KisLsDropShadowFilter::neededRect(const PkRect &rect, KisPSDLayerStyleSP style, KisLayerStyleFilterEnvironment *env) const
{
    const psd_layer_effects_shadow_base *shadowStruct = getShadowStruct(style);
    if (!shadowStruct->effectEnabled()) return rect;

    KisLsUtils::LodWrapper<psd_layer_effects_shadow_base> w(env->currentLevelOfDetail(), shadowStruct);
    ShadowRectsData d(rect, style->context(), w.config, ShadowRectsData::NEED_RECT);
    return rect | d.finalNeedRect();
}

PkRect KisLsDropShadowFilter::changedRect(const PkRect &rect, KisPSDLayerStyleSP style, KisLayerStyleFilterEnvironment *env) const
{
    const psd_layer_effects_shadow_base *shadowStruct = getShadowStruct(style);
    if (!shadowStruct->effectEnabled()) return rect;

    KisLsUtils::LodWrapper<psd_layer_effects_shadow_base> w(env->currentLevelOfDetail(), shadowStruct);
    ShadowRectsData d(rect, style->context(), w.config, ShadowRectsData::CHANGE_RECT);
    return style->context()->keep_original ?
        d.finalChangeRect() : rect | d.finalChangeRect();
}
