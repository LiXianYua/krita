/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// ===========================================================================
// [GAP] kis_ls_stroke_filter.cpp 阻塞登记（S-06 Task 6，修复轮更新）
//
// 本文件不进薄壳，仅剥可机械映射类型（源文件 Q* 已归零）。原「协变返回断裂」
// 阻塞点已由 KoResource 族 PkVector→PkList 修复解除（修复轮验证：编过）。现阻塞：
//   * mid()：include krita_utils.h 的模板 rasterizePolygonDDA 用 Qt 序列容器
//     mid()，PkVector 无此方法，定义期即报错（krita_utils.h:270）。
// 关闭条件：给 PkVector 补 mid()，或 krita_utils.h 该模板改用 Pk 容器接口。
// 当前状态：Qt 仅经未剥依赖头传递进入，不参与薄壳构建。
// ===========================================================================


#include "kis_ls_stroke_filter.h"

#include <cstdlib>

#include <PkBitArray.h>

#include <resources/KoPattern.h>

#include <resources/KoAbstractGradient.h>

#include "psd.h"

#include "kis_convolution_kernel.h"
#include "kis_convolution_painter.h"
#include "kis_gaussian_kernel.h"

#include "kis_pixel_selection.h"
#include "kis_fill_painter.h"
#include "kis_gradient_painter.h"
#include "kis_iterator_ng.h"
#include "kis_random_accessor_ng.h"

#include "kis_psd_layer_style.h"
#include "kis_layer_style_filter_environment.h"

#include "kis_ls_utils.h"
#include "kis_multiple_projection.h"
#include "kis_cached_paint_device.h"
#include "krita_utils.h"
#include "KisLayerStyleKnockoutBlower.h"


namespace {

int borderSize(psd_stroke_position position, int size)
{
    int border = 1;

    // The need rect border should not depend on the position;
    // instead it should extend the area all the time. Otherwise
    // small changes on the borderline will not be propagated to
    // the inner side of the blob. See bug 429165.

    switch (position) {
    case psd_stroke_outside:
    case psd_stroke_inside:
        border = size + 1;
        break;
    case psd_stroke_center:
        border = qCeil(0.5 * size) + 1;
        break;
    }

    return border;
}

}


KisLsStrokeFilter::KisLsStrokeFilter()
    : KisLayerStyleFilter(KoID("lsstroke", PkString("Stroke (style)")))
{
}

KisLsStrokeFilter::KisLsStrokeFilter(const KisLsStrokeFilter &rhs)
    : KisLayerStyleFilter(rhs)
{
}

KisLayerStyleFilter *KisLsStrokeFilter::clone() const
{
    return new KisLsStrokeFilter(*this);
}

void KisLsStrokeFilter::applyStroke(KisPaintDeviceSP srcDevice,
                                    KisMultipleProjection *dst,
                                    KisLayerStyleKnockoutBlower *blower,
                                    const PkRect &applyRect,
                                    const psd_layer_effects_stroke *config,
                                    KisResourcesInterfaceSP resourcesInterface,
                                    KisLayerStyleFilterEnvironment *env) const
{
    if (applyRect.isEmpty()) return;

    const PkRect needRect = kisGrowRect(applyRect, borderSize(config->position(), config->size()));

    KisSelectionSP baseSelection = blower->knockoutSelectionLazy();
    KisPixelSelectionSP selection = baseSelection->pixelSelection();

    KisCachedSelection::Guard s1(*env->cachedSelection());
    KisPixelSelectionSP dilatedSelection = s1.selection()->pixelSelection();
    KisLsUtils::selectionFromAlphaChannel(srcDevice, s1.selection(), needRect);

    {
        KisCachedSelection::Guard s2(*env->cachedSelection());
        KisPixelSelectionSP erodedSelection = s2.selection()->pixelSelection();
        erodedSelection->makeCloneFromRough(dilatedSelection, needRect);

        if (config->position() == psd_stroke_outside) {
            KisGaussianKernel::applyDilate(dilatedSelection, needRect, config->size(), PkBitArray(), 0, true);
        } else if (config->position() == psd_stroke_inside) {
            KisGaussianKernel::applyErodeU8(erodedSelection, needRect, config->size(), PkBitArray(), 0, true);
        } else if (config->position() == psd_stroke_center) {
            KisGaussianKernel::applyDilate(dilatedSelection, needRect, 0.5 * config->size(), PkBitArray(), 0, true);
            KisGaussianKernel::applyErodeU8(erodedSelection, needRect, 0.5 * config->size(), PkBitArray(), 0, true);
        }

        KisPainter gc(selection);

        gc.setCompositeOpId(COMPOSITE_COPY);
        gc.bitBlt(applyRect.topLeft(), dilatedSelection, applyRect);

        gc.setCompositeOpId(COMPOSITE_ERASE);
        gc.bitBlt(applyRect.topLeft(), erodedSelection, applyRect);
        gc.end();
    }

    const PkString compositeOp = config->blendMode();
    const quint8 opacityU8 = quint8(qRound(255.0 / 100.0 * config->opacity()));
    KisPaintDeviceSP dstDevice = dst->getProjection(KisMultipleProjection::defaultProjectionId(),
                                                    compositeOp,
                                                    opacityU8,
                                                    PkBitArray(),
                                                    srcDevice);
    KisLsUtils::fillOverlayDevice(dstDevice, applyRect, config, resourcesInterface, env);
}

void KisLsStrokeFilter::processDirectly(KisPaintDeviceSP src,
                                        KisMultipleProjection *dst,
                                        KisLayerStyleKnockoutBlower *blower,
                                        const PkRect &applyRect,
                                        KisPSDLayerStyleSP style,
                                        KisLayerStyleFilterEnvironment *env) const
{
    Q_UNUSED(env);
    KIS_ASSERT_RECOVER_RETURN(style);

    const psd_layer_effects_stroke *config = style->stroke();
    if (!KisLsUtils::checkEffectEnabled(config, dst)) return;

    KisLsUtils::LodWrapper<psd_layer_effects_stroke> w(env->currentLevelOfDetail(), config);
    applyStroke(src, dst, blower, applyRect, w.config, style->resourcesInterface(), env);
}

PkRect KisLsStrokeFilter::neededRect(const PkRect &rect, KisPSDLayerStyleSP style, KisLayerStyleFilterEnvironment *env) const
{
    const psd_layer_effects_stroke *config = style->stroke();
    if (!config->effectEnabled()) return rect;

    KisLsUtils::LodWrapper<psd_layer_effects_stroke> w(env->currentLevelOfDetail(), config);
    return kisGrowRect(rect, borderSize(w.config->position(), w.config->size()));
}

PkRect KisLsStrokeFilter::changedRect(const PkRect &rect, KisPSDLayerStyleSP style, KisLayerStyleFilterEnvironment *env) const
{
    return neededRect(rect, style, env);
}

KritaUtils::ThresholdMode KisLsStrokeFilter::sourcePlaneOpacityThresholdRequirement(KisPSDLayerStyleSP style) const
{
    const psd_layer_effects_stroke *config = style->stroke();

    if (!config->effectEnabled()) return KritaUtils::ThresholdNone;

    return !config->effectEnabled() || config->position() == psd_stroke_center ? KritaUtils::ThresholdNone :
        config->position() == psd_stroke_inside ? KritaUtils::ThresholdFloor :
        KritaUtils::ThresholdCeil;
}
