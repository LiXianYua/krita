/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// ===========================================================================
// [GAP] kis_colorize_stroke_strategy.cpp 阻塞登记（S-06 Task 6）
//
// 本文件不进薄壳，仅剥可机械映射类型（源文件 Q* 已归零）。阻塞原因：
//   * 必须 include krita_utils.h，其模板 rasterizePolygonDDA 体里用 Qt 序列容器
//     的 mid()，而 PkVector 无 mid()（pk/container 未实现）。该表达式非模板依赖，
//     编译器在定义期即报错，任何 include krita_utils.h 的 TU 都编不过
// 关闭条件：给 PkVector 补 mid()，或 krita_utils.h 该模板改用 Pk 容器接口。
// 当前状态：Qt 仅经未剥依赖头传递进入，不参与薄壳构建。
// ===========================================================================


#include "kis_colorize_stroke_strategy.h"

#include <PkBitArray.h>

#include "krita_utils.h"
#include "kis_paint_device.h"
#include "kis_lazy_fill_tools.h"
#include "kis_gaussian_kernel.h"
#include "kis_painter.h"
#include "kis_default_bounds_base.h"
#include "kis_lod_transform.h"
#include "kis_node.h"
#include "kis_image_config.h"
#include "KisWatershedWorker.h"
#include "kis_processing_visitor.h"

#include "kis_transaction.h"

#include <KisRunnableStrokeJobData.h>
#include <KisRunnableStrokeJobUtils.h>
#include <KisRunnableStrokeJobsInterface.h>

using namespace KisLazyFillTools;

struct KisColorizeStrokeStrategy::Private
{
    Private() : filteredSourceValid(false) {}
    Private(const Private &rhs, int _levelOfDetail)
        : progressNode(rhs.progressNode)
        , src(rhs.src)
        , dst(rhs.dst)
        , filteredSource(rhs.filteredSource)
        , internalFilteredSource(rhs.internalFilteredSource)
        , filteredSourceValid(rhs.filteredSourceValid)
        , boundingRect(rhs.boundingRect)
        , prefilterOnly(rhs.prefilterOnly)
        , levelOfDetail(_levelOfDetail)
        , keyStrokes(rhs.keyStrokes)
        , filteringOptions(rhs.filteringOptions)
    {}

    KisNodeSP progressNode;
    PkSharedPointer<KisProcessingVisitor::ProgressHelper> progressHelper;
    KisPaintDeviceSP src;
    KisPaintDeviceSP dst;
    KisPaintDeviceSP filteredSource;
    KisPaintDeviceSP heightMap;
    KisPaintDeviceSP internalFilteredSource;
    bool filteredSourceValid;
    PkRect boundingRect;

    bool prefilterOnly = false;
    int levelOfDetail = 0;

    PkVector<KeyStroke> keyStrokes;

    // default values: disabled
    FilteringOptions filteringOptions;
};

KisColorizeStrokeStrategy::KisColorizeStrokeStrategy(KisPaintDeviceSP src,
                                                     KisPaintDeviceSP dst,
                                                     KisPaintDeviceSP filteredSource,
                                                     bool filteredSourceValid,
                                                     const PkRect &boundingRect,
                                                     KisNodeSP progressNode,
                                                     bool prefilterOnly)
    : KisRunnableBasedStrokeStrategy(PkString("colorize-stroke"), prefilterOnly ? kundo2_text("Prefilter Colorize Mask") : kundo2_text("Colorize")),
      m_d(new Private)
{
    m_d->progressNode = progressNode;
    m_d->src = src;
    m_d->dst = dst;
    m_d->filteredSource = filteredSource;
    m_d->boundingRect = boundingRect;
    m_d->filteredSourceValid = filteredSourceValid;
    m_d->prefilterOnly = prefilterOnly;

    enableJob(JOB_INIT, true, KisStrokeJobData::SEQUENTIAL, KisStrokeJobData::EXCLUSIVE);
    enableJob(JOB_DOSTROKE, true, KisStrokeJobData::SEQUENTIAL, KisStrokeJobData::EXCLUSIVE);
    enableJob(JOB_CANCEL, true, KisStrokeJobData::SEQUENTIAL, KisStrokeJobData::EXCLUSIVE);

    setNeedsExplicitCancel(true);
    setRequestsOtherStrokesToEnd(false);
    setClearsRedoOnStart(false);
}

KisColorizeStrokeStrategy::KisColorizeStrokeStrategy(const KisColorizeStrokeStrategy &rhs, int levelOfDetail)
    : KisRunnableBasedStrokeStrategy(rhs),
      m_d(new Private(*rhs.m_d, levelOfDetail))
{
    KisLodTransform t(levelOfDetail);
    m_d->boundingRect = t.map(rhs.m_d->boundingRect);
}

KisColorizeStrokeStrategy::~KisColorizeStrokeStrategy()
{
}

void KisColorizeStrokeStrategy::setFilteringOptions(const FilteringOptions &value)
{
    m_d->filteringOptions = value;
}

FilteringOptions KisColorizeStrokeStrategy::filteringOptions() const
{
    return m_d->filteringOptions;
}

void KisColorizeStrokeStrategy::addKeyStroke(KisPaintDeviceSP dev, const KoColor &color)
{
    KoColor convertedColor(color);
    convertedColor.convertTo(m_d->dst->colorSpace());

    m_d->keyStrokes << KeyStroke(dev, convertedColor);
}

void KisColorizeStrokeStrategy::initStrokeCallback()
{
    using namespace KritaUtils;

    PkVector<KisRunnableStrokeJobData*> jobs;

    const PkVector<PkRect> patchRects =
        splitRectIntoPatches(m_d->boundingRect, optimalPatchSize());

    if (!m_d->filteredSourceValid) {
        // TODO: make this conversion concurrent!!!
        KisPaintDeviceSP filteredMainDev = KisPainter::convertToAlphaAsAlpha(m_d->src);
        filteredMainDev->setDefaultBounds(m_d->src->defaultBounds());

        struct PrefilterSharedState {
            PkRect boundingRect;
            KisPaintDeviceSP filteredMainDev;
            KisPaintDeviceSP filteredMainDevSavedCopy;
            PkScopedPointer<KisTransaction> activeTransaction;
            FilteringOptions filteringOptions;
        };

        PkSharedPointer<PrefilterSharedState> state(new PrefilterSharedState());
        state->boundingRect = m_d->boundingRect;
        state->filteredMainDev = filteredMainDev;
        state->filteringOptions = m_d->filteringOptions;

        if (m_d->filteringOptions.useEdgeDetection &&
            m_d->filteringOptions.edgeDetectionSize > 0.0) {

            addJobSequential(jobs, [state] () {
                state->activeTransaction.reset(new KisTransaction(state->filteredMainDev));
            });

            for (const PkRect &rc : patchRects) {
                addJobConcurrent(jobs, [state, rc] () {
                    KisLodTransformScalar t(state->filteredMainDev);
                    KisGaussianKernel::applyLoG(state->filteredMainDev,
                                                rc,
                                                t.scale(0.5 * state->filteringOptions.edgeDetectionSize),
                                                -1.0,
                                                PkBitArray(), 0);
                });
            }

            addJobSequential(jobs, [state] () {
                state->activeTransaction.reset();
                normalizeAlpha8Device(state->filteredMainDev, state->boundingRect);
                state->activeTransaction.reset(new KisTransaction(state->filteredMainDev));
            });

            for (const PkRect &rc : patchRects) {
                addJobConcurrent(jobs, [state, rc] () {
                    KisLodTransformScalar t(state->filteredMainDev);
                    KisGaussianKernel::applyGaussian(state->filteredMainDev,
                                                     rc,
                                                     t.scale(state->filteringOptions.edgeDetectionSize),
                                                     t.scale(state->filteringOptions.edgeDetectionSize),
                                                     PkBitArray(), 0);
                });
            }

            addJobSequential(jobs, [state] () {
                state->activeTransaction.reset();
            });
        }

        if (m_d->filteringOptions.fuzzyRadius > 0) {

            addJobSequential(jobs, [state] () {
                state->filteredMainDevSavedCopy = new KisPaintDevice(*state->filteredMainDev);
                state->activeTransaction.reset(new KisTransaction(state->filteredMainDev));
            });

            for (const PkRect &rc : patchRects) {
                addJobConcurrent(jobs, [state, rc] () {
                    KisLodTransformScalar t(state->filteredMainDev);
                    KisGaussianKernel::applyGaussian(state->filteredMainDev,
                                                     rc,
                                                     t.scale(state->filteringOptions.fuzzyRadius),
                                                     t.scale(state->filteringOptions.fuzzyRadius),
                                                     PkBitArray(), 0);
                    KisPainter gc(state->filteredMainDev);
                    gc.bitBlt(rc.topLeft(), state->filteredMainDevSavedCopy, rc);
                });
            }

            addJobSequential(jobs, [state] () {
                state->activeTransaction.reset();
            });
        }

        addJobSequential(jobs, [this, state] () {
            normalizeAndInvertAlpha8Device(state->filteredMainDev, state->boundingRect);

            KisDefaultBoundsBaseSP oldBounds = m_d->filteredSource->defaultBounds();
            m_d->filteredSource->makeCloneFrom(state->filteredMainDev, m_d->boundingRect);
            m_d->filteredSource->setDefaultBounds(oldBounds);
            m_d->filteredSourceValid = true;
        });
    }

    if (!m_d->prefilterOnly) {
        addJobSequential(jobs, [this] () {
            m_d->heightMap = new KisPaintDevice(*m_d->filteredSource);
        });

        for (const PkRect &rc : patchRects) {
            addJobConcurrent(jobs, [this, rc] () {
                KritaUtils::filterAlpha8Device(m_d->heightMap, rc,
                                               [](quint8 pixel) {
                                                   return quint8(255 - pixel);
                                               });
            });
        }

        addJobSequential(jobs, [this] () {
            m_d->progressHelper.reset(new KisProcessingVisitor::ProgressHelper(m_d->progressNode));

            KisWatershedWorker worker(m_d->heightMap, m_d->dst, m_d->boundingRect, m_d->progressHelper->updater());
            for (const KeyStroke &stroke : m_d->keyStrokes) {
                KoColor color =
                    !stroke.isTransparent ?
                    stroke.color : KoColor::createTransparent(m_d->dst->colorSpace());

                worker.addKeyStroke(stroke.dev, color);
            }
            worker.run(m_d->filteringOptions.cleanUpAmount);
            m_d->progressHelper.reset();
        });
    }

    addJobSequential(jobs, [this] () {
        Q_EMIT sigFinished(m_d->prefilterOnly);
    });

    runnableJobsInterface()->addRunnableJobs(jobs);
}

void KisColorizeStrokeStrategy::cancelStrokeCallback()
{
    Q_EMIT sigCancelled();
}

void KisColorizeStrokeStrategy::tryCancelCurrentStrokeJobAsync()
{
    // NOTE: this method may be called by the GUI thread asynchronously!
    PkSharedPointer<KisProcessingVisitor::ProgressHelper> helper = m_d->progressHelper;
    if (helper) {
        helper->cancel();
    }
}

KisStrokeStrategy* KisColorizeStrokeStrategy::createLodClone(int levelOfDetail)
{
    KisImageConfig cfg(true);
    if (!cfg.useLodForColorizeMask()) return 0;

    KisColorizeStrokeStrategy *clone = new KisColorizeStrokeStrategy(*this, levelOfDetail);
    return clone;
}
