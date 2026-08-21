/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2020 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// ===========================================================================
// [GAP] kis_generator_stroke_strategy.cpp 阻塞登记（S-06 Task 6，修复轮更新）
//
// 本文件不进薄壳，仅剥可机械映射类型（源文件 Q* 已归零）。原「协变返回断裂」
// 阻塞点已由 KoResource 族 PkVector→PkList 修复解除（修复轮验证：编过）。现阻塞：
//   * mid()：include krita_utils.h 的模板 rasterizePolygonDDA 用 Qt 序列容器
//     mid()，PkVector 无此方法，定义期即报错（krita_utils.h:270）。
// 关闭条件：给 PkVector 补 mid()，或 krita_utils.h 该模板改用 Pk 容器接口。
// 当前状态：Qt 仅经未剥依赖头传递进入，不参与薄壳构建。
// ===========================================================================

#include <KisRunnableStrokeJobUtils.h>
#include <filter/kis_filter_configuration.h>
#include <kis_generator_layer.h>
#include <kis_processing_information.h>
#include <kis_processing_visitor.h>
#include <kis_selection.h>
#include <krita_utils.h>

#include "kis_generator_stroke_strategy.h"

KisGeneratorStrokeStrategy::KisGeneratorStrokeStrategy()
    : KisRunnableBasedStrokeStrategy(PkString("KisGenerator"), kundo2_text("Fill Layer Render"))
{
    enableJob(KisSimpleStrokeStrategy::JOB_INIT, true, KisStrokeJobData::BARRIER, KisStrokeJobData::EXCLUSIVE);
    enableJob(KisSimpleStrokeStrategy::JOB_DOSTROKE);

    setRequestsOtherStrokesToEnd(false);
    setClearsRedoOnStart(false);
    setCanForgetAboutMe(false);
}

PkVector<KisStrokeJobData *>KisGeneratorStrokeStrategy::createJobsData(const KisGeneratorLayerSP layer, PkSharedPointer<boost::none_t> cookie, const KisGeneratorSP f, const KisPaintDeviceSP dev, const PkRegion &region, const KisFilterConfigurationSP filterConfig)
{
    using namespace KritaUtils;

    PkVector<KisStrokeJobData *> jobsData;

    PkSharedPointer<KisProcessingVisitor::ProgressHelper> helper(new KisProcessingVisitor::ProgressHelper(layer));

    addJobBarrier(jobsData, nullptr);

    for (const auto& rc: region) {
        if (f->allowsSplittingIntoPatches()) {
            PkVector<PkRect> tiles = splitRectIntoPatches(rc, optimalPatchSize());

            for(const auto& tile: tiles) {
                KisProcessingInformation dstCfg(dev, tile.topLeft(), KisSelectionSP());
                addJobConcurrent(jobsData, [=]() {
                    f->generate(dstCfg, tile.size(), filterConfig, helper->updater());

                    // HACK ALERT!!!
                    // this avoids cyclic loop with KisRecalculateGeneratorLayerJob::run()
                    const_cast<KisGeneratorLayerSP &>(layer)->setDirtyWithoutUpdate({tile});

                    const_cast<PkSharedPointer<boost::none_t> &>(cookie).clear();
                });
            }
        } else {
            KisProcessingInformation dstCfg(dev, rc.topLeft(), KisSelectionSP());

            addJobSequential(jobsData, [=]() {
                f->generate(dstCfg, rc.size(), filterConfig, helper->updater());

                // HACK ALERT!!!
                // this avoids cyclic loop with KisRecalculateGeneratorLayerJob::run()
                const_cast<KisGeneratorLayerSP &>(layer)->setDirtyWithoutUpdate({rc});

                const_cast<PkSharedPointer<boost::none_t>&>(cookie).clear();
            });
        }
    }

    return jobsData;
}
KisGeneratorStrokeStrategy::~KisGeneratorStrokeStrategy()
{
}
