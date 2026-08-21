/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// ===========================================================================
// [GAP] kis_ls_overlay_filter.cpp 阻塞登记（S-06 Task 6）
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


#include "kis_ls_overlay_filter.h"

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



KisLsOverlayFilter::KisLsOverlayFilter(Mode mode)
    : KisLayerStyleFilter(KoID("lsoverlay", PkString("Overlay (style)"))),
      m_mode(mode)
{
}

KisLsOverlayFilter::KisLsOverlayFilter(const KisLsOverlayFilter &rhs)
    : KisLayerStyleFilter(rhs),
      m_mode(rhs.m_mode)
{
}

KisLayerStyleFilter *KisLsOverlayFilter::clone() const
{
    return new KisLsOverlayFilter(*this);
}

void KisLsOverlayFilter::applyOverlay(KisPaintDeviceSP srcDevice,
                                      KisMultipleProjection *dst,
                                      const PkRect &applyRect,
                                      const psd_layer_effects_overlay_base *config,
                                      KisResourcesInterfaceSP resourcesInterface,
                                      KisLayerStyleFilterEnvironment *env) const
{
    if (applyRect.isEmpty()) return;

    const PkString compositeOp = config->blendMode();
    const quint8 opacityU8 = quint8(qRound(255.0 / 100.0 * config->opacity()));

    KisPaintDeviceSP dstDevice = dst->getProjection(KisMultipleProjection::defaultProjectionId(),
                                                    compositeOp,
                                                    opacityU8,
                                                    PkBitArray(),
                                                    srcDevice);

    KisLsUtils::fillOverlayDevice(dstDevice, applyRect, config, resourcesInterface, env);
}

const psd_layer_effects_overlay_base*
KisLsOverlayFilter::getOverlayStruct(KisPSDLayerStyleSP style) const
{
    const psd_layer_effects_overlay_base *config = 0;

    if (m_mode == Color) {
        config = style->colorOverlay();
    } else if (m_mode == Gradient) {
        config = style->gradientOverlay();
    } else if (m_mode == Pattern) {
        config = style->patternOverlay();
    }

    return config;
}

void KisLsOverlayFilter::processDirectly(KisPaintDeviceSP src,
                                         KisMultipleProjection *dst,
                                         KisLayerStyleKnockoutBlower *blower,
                                         const PkRect &applyRect,
                                         KisPSDLayerStyleSP style,
                                         KisLayerStyleFilterEnvironment *env) const
{
    Q_UNUSED(env);
    Q_UNUSED(blower);
    KIS_ASSERT_RECOVER_RETURN(style);

    const psd_layer_effects_overlay_base *config = getOverlayStruct(style);
    if (!KisLsUtils::checkEffectEnabled(config, dst)) return;

    applyOverlay(src, dst, applyRect, config, style->resourcesInterface(), env);
}

PkRect KisLsOverlayFilter::neededRect(const PkRect &rect, KisPSDLayerStyleSP style, KisLayerStyleFilterEnvironment *env) const
{
    Q_UNUSED(style);
    Q_UNUSED(env);
    return rect;
}

PkRect KisLsOverlayFilter::changedRect(const PkRect &rect, KisPSDLayerStyleSP style, KisLayerStyleFilterEnvironment *env) const
{
    Q_UNUSED(style);
    Q_UNUSED(env);
    return rect;
}
