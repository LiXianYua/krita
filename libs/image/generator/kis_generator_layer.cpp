/*
 *  SPDX-FileCopyrightText: 2008 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2020 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// ===========================================================================
// [GAP] kis_generator_layer.cpp 阻塞登记（S-06 Task 6，修复轮更新）
//
// 本文件不进薄壳。源文件 Q* 已归零、机械替换已完成，**单独编译通过**（无 mid()
// 报错，原「兼 mid」预判不成立）。现阻塞在**符号层**：本 TU 发射
// KisGeneratorLayer 的 vtable，引用两个仍未剥的缩略图虚函数
// KisSelectionBasedLayer::createThumbnail 与 KisLayer::createThumbnailForFrame
// （声明见 kis_selection_based_layer.h:153 / kis_layer.h:251/255），签名带
// Qt::AspectRatioMode（Qt 类型未剥），链接层 `nm -u | grep -i qt` 出现 Qt 符号，
// 违反 S 线 L3 判据。
// 关闭条件：Task 8 剥到这两个缩略图虚函数（Qt::AspectRatioMode → Pk 枚举、
// 返回类型 Q* 图像 → Pk 图像类型）后解除。
//

#include <PkMutex.h>
#include <PkRegion.h>

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

struct KisGeneratorLayer::Private
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
