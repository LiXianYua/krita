/*
 *  SPDX-FileCopyrightText: 2021 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// ===========================================================================
// [GAP] KisStrokeLayerStyleFilterProjectionPlane.cpp 阻塞登记（S-06 Task 6，修复轮更新）
//
// 本文件不进薄壳，仅剥可机械映射类型（源文件 Q* 已归零）。阻塞原因：
//   * mid()：传递 include kis_ls_stroke_filter.h → krita_utils.h，其模板
//     rasterizePolygonDDA 体里用 Qt 序列容器 mid()，PkVector 无此方法，定义期报错。
// 关闭条件：给 PkVector 补 mid()，或 krita_utils.h 该模板改用 Pk 容器接口。
// 当前状态：Qt 仅经未剥依赖头传递进入，不参与薄壳构建。
// ===========================================================================


#include "KisStrokeLayerStyleFilterProjectionPlane.h"

#include "kis_ls_stroke_filter.h"

KisStrokeLayerStyleFilterProjectionPlane::KisStrokeLayerStyleFilterProjectionPlane(KisLayer *sourceLayer)
    : KisLayerStyleFilterProjectionPlane(sourceLayer)
{
}

KisStrokeLayerStyleFilterProjectionPlane::KisStrokeLayerStyleFilterProjectionPlane(const KisStrokeLayerStyleFilterProjectionPlane &rhs, KisLayer *sourceLayer, KisPSDLayerStyleSP clonedStyle)
    : KisLayerStyleFilterProjectionPlane(rhs, sourceLayer, clonedStyle)
{
}

KisStrokeLayerStyleFilterProjectionPlane::~KisStrokeLayerStyleFilterProjectionPlane()
{
}

KritaUtils::ThresholdMode KisStrokeLayerStyleFilterProjectionPlane::sourcePlaneOpacityThresholdRequirement() const
{
    if (!filter()) return KritaUtils::ThresholdNone;

    const KisLsStrokeFilter *filter = dynamic_cast<const KisLsStrokeFilter*>(this->filter());
    return filter ? filter->sourcePlaneOpacityThresholdRequirement(style()) : KritaUtils::ThresholdNone;
}
