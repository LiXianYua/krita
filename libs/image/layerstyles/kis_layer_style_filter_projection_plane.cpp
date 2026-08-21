/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// ===========================================================================
// [GAP] kis_layer_style_filter_projection_plane.cpp 阻塞登记（S-06 Task 6）
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


#include "kis_layer_style_filter_projection_plane.h"

#include "filter/kis_filter.h"
#include "filter/kis_filter_configuration.h"
#include "filter/kis_filter_registry.h"
#include "kis_layer_style_filter.h"
#include "kis_layer_style_filter_environment.h"
#include "kis_psd_layer_style.h"


#include "kis_painter.h"
#include "kis_multiple_projection.h"
#include "KisLayerStyleKnockoutBlower.h"


struct KisLayerStyleFilterProjectionPlane::Private
{
    Private(KisLayer *_sourceLayer)
        : sourceLayer(_sourceLayer),
          environment(new KisLayerStyleFilterEnvironment(_sourceLayer))
    {
        KIS_SAFE_ASSERT_RECOVER_NOOP(_sourceLayer);
    }

    Private(const Private &rhs, KisLayer *_sourceLayer, KisPSDLayerStyleSP clonedStyle)
        : sourceLayer(_sourceLayer),
          filter(rhs.filter ? rhs.filter->clone() : 0),
          style(clonedStyle),
          environment(new KisLayerStyleFilterEnvironment(_sourceLayer)),
          knockoutBlower(rhs.knockoutBlower),
          projection(rhs.projection)
    {
        KIS_SAFE_ASSERT_RECOVER_NOOP(_sourceLayer);
    }


    KisLayer *sourceLayer;

    PkScopedPointer<KisLayerStyleFilter> filter;
    KisPSDLayerStyleSP style;
    PkScopedPointer<KisLayerStyleFilterEnvironment> environment;
    KisLayerStyleKnockoutBlower knockoutBlower;

    KisMultipleProjection projection;
};

KisLayerStyleFilterProjectionPlane::
KisLayerStyleFilterProjectionPlane(KisLayer *sourceLayer)
    : m_d(new Private(sourceLayer))
{
}

KisLayerStyleFilterProjectionPlane::KisLayerStyleFilterProjectionPlane(const KisLayerStyleFilterProjectionPlane &rhs, KisLayer *sourceLayer, KisPSDLayerStyleSP clonedStyle)
    : m_d(new Private(*rhs.m_d, sourceLayer, clonedStyle))
{
}

KisLayerStyleFilterProjectionPlane::~KisLayerStyleFilterProjectionPlane()
{
}

void KisLayerStyleFilterProjectionPlane::setStyle(KisLayerStyleFilter *filter, KisPSDLayerStyleSP style)
{
    m_d->filter.reset(filter);
    m_d->style = style;
}

PkRect KisLayerStyleFilterProjectionPlane::recalculate(const PkRect& rect, KisNodeSP filthyNode, KisRenderPassFlags flags)
{
    Q_UNUSED(filthyNode);
    Q_UNUSED(flags);

    if (!m_d->sourceLayer || !m_d->filter) {
        warnKrita << "KisLayerStyleFilterProjectionPlane::recalculate(): [BUG] is not initialized";
        return PkRect(0, 0, 0, 0);
    }

    m_d->projection.clear(rect);
    m_d->filter->processDirectly(m_d->sourceLayer->projection(),
                                 &m_d->projection,
                                 &m_d->knockoutBlower,
                                 rect,
                                 m_d->style,
                                 m_d->environment.data());
    return rect;
}

void KisLayerStyleFilterProjectionPlane::apply(KisPainter *painter, const PkRect &rect)
{
    m_d->projection.apply(painter->device(), rect, m_d->environment.data());
}

KisPaintDeviceList KisLayerStyleFilterProjectionPlane::getLodCapableDevices() const
{
    return m_d->projection.getLodCapableDevices();
}

bool KisLayerStyleFilterProjectionPlane::isEmpty() const
{
    return m_d->projection.isEmpty();
}

KisLayerStyleKnockoutBlower *KisLayerStyleFilterProjectionPlane::knockoutBlower() const
{
    return &m_d->knockoutBlower;
}

KisLayerStyleFilter *KisLayerStyleFilterProjectionPlane::filter() const
{
    return m_d->filter.data();
}

KisPSDLayerStyleSP KisLayerStyleFilterProjectionPlane::style() const
{
    return m_d->style;
}

PkRect KisLayerStyleFilterProjectionPlane::needRect(const PkRect &rect, KisLayer::PositionToFilthy pos) const
{
    if (!m_d->sourceLayer || !m_d->filter) {
        warnKrita << "KisLayerStyleFilterProjectionPlane::needRect(): [BUG] is not initialized";
        return rect;
    }

    KIS_ASSERT_RECOVER_NOOP(pos == KisLayer::N_ABOVE_FILTHY);
    return m_d->filter->neededRect(rect, m_d->style, m_d->environment.data());
}

PkRect KisLayerStyleFilterProjectionPlane::changeRect(const PkRect &rect, KisLayer::PositionToFilthy pos) const
{
    if (!m_d->sourceLayer || !m_d->filter) {
        warnKrita << "KisLayerStyleFilterProjectionPlane::changeRect(): [BUG] is not initialized";
        return rect;
    }

    KIS_ASSERT_RECOVER_NOOP(pos == KisLayer::N_ABOVE_FILTHY);
    return m_d->filter->changedRect(rect, m_d->style, m_d->environment.data());
}

PkRect KisLayerStyleFilterProjectionPlane::accessRect(const PkRect &rect, KisLayer::PositionToFilthy pos) const
{
    return needRect(rect, pos);
}

PkRect KisLayerStyleFilterProjectionPlane::needRectForOriginal(const PkRect &rect) const
{
    return needRect(rect, KisLayer::N_ABOVE_FILTHY);
}

PkRect KisLayerStyleFilterProjectionPlane::tightUserVisibleBounds() const
{
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(m_d->filter, PkRect(0, 0, 0, 0));
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(m_d->sourceLayer, PkRect(0, 0, 0, 0));

    return m_d->filter->changedRect(m_d->sourceLayer->exactBounds(),
                                    m_d->style,
                                    m_d->environment.data());
}

PkRect KisLayerStyleFilterProjectionPlane::looseUserVisibleBounds() const
{
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(m_d->filter, PkRect(0, 0, 0, 0));
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(m_d->sourceLayer, PkRect(0, 0, 0, 0));

    return m_d->filter->changedRect(m_d->sourceLayer->extent(),
                                    m_d->style,
                                    m_d->environment.data());
}
