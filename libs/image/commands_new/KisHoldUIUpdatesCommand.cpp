/*
 *  SPDX-FileCopyrightText: 2019 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// ===========================================================================
// [GAP] KisHoldUIUpdatesCommand.cpp 阻塞登记（S-06 Task 5）
//
// 本文件不进薄壳，仅剥可机械映射类型（源文件 Q* 已归零）。阻塞原因：
//   * include krita_utils.h → rasterizePolygonDDA 模板用 Qt 序列容器的 mid()
//     （PkVector 无），定义期编译失败（与 Task 4 kis_paintop_utils.cpp 同因）
//   * 另缺 #include <numeric>（std::accumulate）—— 已补，待 [GAP] 关闭后即可编过
// 关闭条件：给 PkVector 补 mid() 或 krita_utils.h 该模板改用 Pk 容器接口。


#include "KisHoldUIUpdatesCommand.h"

#include <algorithm>
#include "kis_image_interfaces.h"
#include "krita_utils.h"
#include "kis_paintop_utils.h"
#include "kis_image_signal_router.h"
#include "KisRunnableStrokeJobData.h"
#include "KisRunnableStrokeJobUtils.h"
#include "KisRunnableStrokeJobsInterface.h"
#include <numeric>

KisHoldUIUpdatesCommand::KisHoldUIUpdatesCommand(KisUpdatesFacade *updatesFacade, State state)
    : KisCommandUtils::FlipFlopCommand(state),
      m_updatesFacade(updatesFacade),
      m_batchUpdateStarted(new bool(false))
{
}

void KisHoldUIUpdatesCommand::partA()
{
    if (*m_batchUpdateStarted) {
        m_updatesFacade->notifyBatchUpdateEnded();
        *m_batchUpdateStarted = false;
    }

    m_updatesFacade->disableUIUpdates();
}

void KisHoldUIUpdatesCommand::partB()
{
    PkVector<PkRect> totalDirtyRects = m_updatesFacade->enableUIUpdates();

    const PkRect totalRect =
        m_updatesFacade->bounds() &
        std::accumulate(totalDirtyRects.begin(), totalDirtyRects.end(), PkRect(0, 0, 0, 0), std::bit_or<PkRect>());

    totalDirtyRects =
        KisPaintOpUtils::splitAndFilterDabRect(totalRect,
                                               totalDirtyRects,
                                               KritaUtils::optimalPatchSize().width());

    *m_batchUpdateStarted = true;
    m_updatesFacade->notifyBatchUpdateStarted();

    KisUpdatesFacade *updatesFacade = m_updatesFacade;
    PkSharedPointer<bool> batchUpdateStarted = m_batchUpdateStarted;

    PkVector<KisRunnableStrokeJobDataBase*> jobsData;
    for (const PkRect &rc : totalDirtyRects) {
        KritaUtils::addJobConcurrent(jobsData, [updatesFacade, rc] () {
            updatesFacade->notifyUIUpdateCompleted(rc);
        });
    }

    KritaUtils::addJobBarrier(jobsData, [updatesFacade, batchUpdateStarted] () {
        updatesFacade->notifyBatchUpdateEnded();
        *batchUpdateStarted = false;
    });

    runnableJobsInterface()->addRunnableJobs(jobsData);
}
