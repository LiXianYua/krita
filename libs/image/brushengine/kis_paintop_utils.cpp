/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// ===========================================================================
// [GAP] kis_paintop_utils.cpp 阻塞登记（S-06 Task 4）
//
// 本文件不进薄壳，仅剥可机械映射类型（源文件 Q* 已归零）。阻塞原因：
//   * 本文件用 KritaUtils::splitRectIntoPatches / filterContainer，必须 include
//     krita_utils.h / krita_container_utils.h
//   * krita_utils.h 的模板 rasterizePolygonDDA 体里用 Qt 序列容器的 mid()，而 PkVector
//     无 mid()（pk/container 未实现）。该表达式非模板依赖，编译器在定义期即报错，
//     任何 include krita_utils.h 的 TU 都编不过，与是否实例化无关
// 关闭条件：krita_utils.h 剥离（改写 rasterizePolygonDDA 不用 mid()）或 pk/container
// 给 PkVector 补 mid()。当前状态：签名已与剥离后的头文件对齐，Qt 仅经未剥的
// krita_utils.h 模板传入，不参与薄壳构建。
// ===========================================================================

#include "kis_paintop_utils.h"

#include "krita_utils.h"
#include "krita_container_utils.h"
#include <KisRenderedDab.h>
#include <PkSize.h>

#include <functional>
#include <numeric>

namespace KisPaintOpUtils {


KisSpacingInformation effectiveSpacing(qreal dabWidth, qreal dabHeight, qreal extraScale, bool distanceSpacingEnabled, bool isotropicSpacing, qreal rotation, bool axesFlipped, qreal spacingVal, bool autoSpacingActive, qreal autoSpacingCoeff, qreal lodScale)
{
    PkPointF spacing;

    if (!isotropicSpacing) {
        if (autoSpacingActive) {
            spacing = calcAutoSpacing(PkPointF(dabWidth, dabHeight), autoSpacingCoeff, lodScale);
        } else {
            spacing = PkPointF(dabWidth, dabHeight);
            spacing *= spacingVal;
        }
    }
    else {
        qreal significantDimension = qMax(dabWidth, dabHeight);
        if (autoSpacingActive) {
            significantDimension = calcAutoSpacing(significantDimension, autoSpacingCoeff);
        } else {
            significantDimension *= spacingVal;
        }
        spacing = PkPointF(significantDimension, significantDimension);
        rotation = 0.0;
        axesFlipped = false;
    }

    spacing *= extraScale;

    return KisSpacingInformation(distanceSpacingEnabled, spacing, rotation, axesFlipped);
}

KisTimingInformation effectiveTiming(bool timingEnabled, qreal timingInterval, qreal rateExtraScale)
{

    if (!timingEnabled) {
        return KisTimingInformation();
    }
    else {
        qreal scaledInterval = rateExtraScale <= 0.0 ? LONG_TIME : timingInterval / rateExtraScale;
        return KisTimingInformation(scaledInterval);
    }
}

PkVector<PkRect> splitAndFilterDabRect(const PkRect &totalRect, const PkVector<PkRect> &dabRects, int idealPatchSize)
{
    PkVector<PkRect> rects = KritaUtils::splitRectIntoPatches(totalRect, PkSize(idealPatchSize,idealPatchSize));

    KritaUtils::filterContainer(rects,
        [dabRects] (const PkRect &rc) {
            for (const PkRect &dab : dabRects) {
                if (dab.intersects(rc)) {
                    return true;
                }
            }
            return false;
        });
    return rects;
}

PkVector<PkRect> splitDabsIntoRects(const PkVector<PkRect> &dabRects, int idealNumRects, int diameter, qreal spacing)
{
    const PkRect totalRect =
        std::accumulate(dabRects.begin(), dabRects.end(), PkRect(), std::bit_or<PkRect>());

    constexpr int minPatchSize = 128;
    constexpr int maxPatchSize = 512;
    constexpr int patchStep = 64;
    constexpr int halfPatchStep = patchStep >> 1;


    int idealPatchSize = qBound(minPatchSize,
                                (int(diameter * (2.0 - spacing)) + halfPatchStep) & ~(patchStep - 1),
                                maxPatchSize);


    PkVector<PkRect> rects = splitAndFilterDabRect(totalRect, dabRects, idealPatchSize);

    while (rects.size() < idealNumRects && idealPatchSize >minPatchSize) {
        idealPatchSize = qMax(minPatchSize, idealPatchSize - patchStep);
        rects = splitAndFilterDabRect(totalRect, dabRects, idealPatchSize);
    }

    return rects;
}



}
