/*
 *  SPDX-FileCopyrightText: 2008 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2020 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// ===========================================================================
// [GAP] kis_generator_layer.cpp 阻塞登记（S-06 Task 6）
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


#include <PkMutex.h>

#include "kis_generator_layer.h"

#include "kis_debug.h"

#include "kis_selection.h"
#include "filter/kis_filter_configuration.h"
#include "kis_processing_information.h"
#include <kis_processing_visitor.h>
#include "generator/kis_generator_registry.h"
#include "generator/kis_generator.h"
#include "kis_node_visitor.h"
#include "kis_thread_safe_signal_compressor.h"
#include <kis_generator_stroke_strategy.h>
#include <KisRunnableStrokeJobData.h>


#define UPDATE_DELAY 100 /*ms */

struct Q_DECL_HIDDEN KisGeneratorLayer::Private
{
    Private()
        : updateSignalCompressor(UPDATE_DELAY, KisSignalCompressor::FIRST_INACTIVE)
    {
    }

    KisThreadSafeSignalCompressor updateSignalCompressor;
    PkRect preparedRect;
    PkRect preparedImageBounds;
    KisFilterConfigurationSP preparedForFilter;
    PkWeakPointer<boost::none_t> updateCookie;
    PkMutex mutex;
};


KisGeneratorLayer::KisGeneratorLayer(KisImageWSP image,
                                     const PkString &name,
                                     KisFilterConfigurationSP kfc,
                                     KisSelectionSP selection)
    : KisSelectionBasedLayer(image, name, selection, kfc),
      m_d(new Private)
{
    PkObject::connect(&m_d->updateSignalCompressor, &KisThreadSafeSignalCompressor::timeout, this, &KisGeneratorLayer::slotDelayedStaticUpdate);
}

KisGeneratorLayer::KisGeneratorLayer(const KisGeneratorLayer& rhs)
    : KisSelectionBasedLayer(rhs),
      m_d(new Private)
{
    PkObject::connect(&m_d->updateSignalCompressor, &KisThreadSafeSignalCompressor::timeout, this, &KisGeneratorLayer::slotDelayedStaticUpdate);
}

KisGeneratorLayer::~KisGeneratorLayer()
{
}

void KisGeneratorLayer::setFilter(KisFilterConfigurationSP filterConfig, bool checkCompareConfig)
{
    setFilterWithoutUpdate(filterConfig, checkCompareConfig);
    m_d->updateSignalCompressor.start();
}

void KisGeneratorLayer::setFilterWithoutUpdate(KisFilterConfigurationSP filterConfig, bool checkCompareConfig)
{
    if (filter().isNull() || (!checkCompareConfig || !filter()->compareTo(filterConfig.constData()))) {
        KisSelectionBasedLayer::setFilter(filterConfig);
        {
            PkMutexLocker locker(&m_d->mutex);
            m_d->preparedRect = PkRect(0, 0, 0, 0);
        }
    }
}

void KisGeneratorLayer::slotDelayedStaticUpdate()
{
    /**
     * Don't try to start a regeneration stroke while image
     * is locked. It may happen on loading, when all necessary
     * conversions are not yet finished.
     */
    if (KisImageSP image = this->image(); image && image->locked()) {
        m_d->updateSignalCompressor.start();
        return;
    }

    /**
     * The mask might have been deleted from the layers stack in the
     * meanwhile. Just ignore the updates in the case.
     */

    KisLayerSP parentLayer(dynamic_cast<KisLayer*>(parent().data()));
    if (!parentLayer) return;

    KisImageSP image = parentLayer->image();

    if (image) {
        if (!m_d->updateCookie) {
            this->update();
        } else {
            m_d->updateSignalCompressor.start();
        }
    }
}

void KisGeneratorLayer::requestUpdateJobsWithStroke(KisStrokeId strokeId, KisFilterConfigurationSP filterConfig)
{
    PkMutexLocker locker(&m_d->mutex);
    
    KisImageSP image = this->image().toStrongRef();
    const PkRect updateRect = extent() | image->bounds();

    if (filterConfig != m_d->preparedForFilter) {
        locker.unlock();
        resetCacheWithoutUpdate(image->colorSpace());
        locker.relock();
    }

    if (m_d->preparedImageBounds != image->bounds()) {
        m_d->preparedRect = PkRect(0, 0, 0, 0);
    }

    const PkRegion processRegion(PkRegion(updateRect) - m_d->preparedRect);
    if (processRegion.isEmpty())
        return;

    KisGeneratorSP f = KisGeneratorRegistry::instance()->value(filterConfig->name());
    KIS_SAFE_ASSERT_RECOVER_RETURN(f);

    KisPaintDeviceSP originalDevice = original();

    PkSharedPointer<boost::none_t> cookie(new boost::none_t(boost::none));

    auto jobs = KisGeneratorStrokeStrategy::createJobsData(this, cookie, f, originalDevice, processRegion, filterConfig);

    for (auto job : jobs) {
        image->addJob(strokeId, job);
    }

    m_d->updateCookie = cookie;
    m_d->preparedRect = updateRect;
    m_d->preparedImageBounds = image->bounds();
    m_d->preparedForFilter = filterConfig;
}

PkWeakPointer<boost::none_t> KisGeneratorLayer::previewWithStroke(const KisStrokeId strokeId)
{
    KisFilterConfigurationSP filterConfig = filter();
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(filterConfig, PkWeakPointer<boost::none_t>());

    requestUpdateJobsWithStroke(strokeId, filterConfig);
    return m_d->updateCookie;
}

void KisGeneratorLayer::update()
{
    KisImageSP image = this->image().toStrongRef();

    KisFilterConfigurationSP filterConfig = filter();
    KIS_SAFE_ASSERT_RECOVER_RETURN(filterConfig);

    KisGeneratorStrokeStrategy *stroke = new KisGeneratorStrokeStrategy();

    KisStrokeId strokeId = image->startStroke(stroke);

    requestUpdateJobsWithStroke(strokeId, filterConfig);

    image->endStroke(strokeId);
}

bool KisGeneratorLayer::accept(KisNodeVisitor & v)
{
    return v.visit(this);
}

void KisGeneratorLayer::accept(KisProcessingVisitor &visitor, KisUndoAdapter *undoAdapter)
{
    return visitor.visit(this, undoAdapter);
}

KisBaseNode::PropertyList KisGeneratorLayer::sectionModelProperties() const
{
    KisFilterConfigurationSP filterConfig = filter();

    KisBaseNode::PropertyList l = KisLayer::sectionModelProperties();
    l << KisBaseNode::Property(KoID("generator", PkString("Generator")),
                               KisGeneratorRegistry::instance()->value(filterConfig->name())->name());

    return l;
}

void KisGeneratorLayer::setX(qint32 x)
{
    KisSelectionBasedLayer::setX(x);
    {
        PkMutexLocker locker(&m_d->mutex);
        m_d->preparedRect = PkRect(0, 0, 0, 0);
    }
    m_d->updateSignalCompressor.start();
}

void KisGeneratorLayer::setY(qint32 y)
{
    KisSelectionBasedLayer::setY(y);
    {
        PkMutexLocker locker(&m_d->mutex);
        m_d->preparedRect = PkRect(0, 0, 0, 0);
    }
    m_d->updateSignalCompressor.start();
}

void KisGeneratorLayer::resetCache(const KoColorSpace *colorSpace)
{
    resetCacheWithoutUpdate(colorSpace);
    m_d->updateSignalCompressor.start();
}

void KisGeneratorLayer::forceUpdateTimedNode()
{
    if (hasPendingTimedUpdates()) {
        m_d->updateSignalCompressor.stop();
        m_d->updateCookie = PkWeakPointer<boost::none_t>();

        slotDelayedStaticUpdate();
    }
}

bool KisGeneratorLayer::hasPendingTimedUpdates() const
{
    return m_d->updateSignalCompressor.isActive();
}

void KisGeneratorLayer::resetCacheWithoutUpdate(const KoColorSpace *colorSpace)
{
    KisSelectionBasedLayer::resetCache(colorSpace);
    {
        PkMutexLocker locker(&m_d->mutex);
        m_d->preparedRect = PkRect(0, 0, 0, 0);
    }
}

void KisGeneratorLayer::setDirty(const PkVector<PkRect> &rects)
{
    setDirtyWithoutUpdate(rects);
    m_d->updateSignalCompressor.start();
}

void KisGeneratorLayer::setDirtyWithoutUpdate(const PkVector<PkRect> &rects)
{
    KisSelectionBasedLayer::setDirty(rects);
}
