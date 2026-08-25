/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_STROKE_STRATEGY_FACTORY_H
#define __KIS_STROKE_STRATEGY_FACTORY_H

#include <functional>
#include <PkContainerAlgo.h>
using KisStrokeStrategyFactory = std::function<KisStrokeStrategy*()>;

using KisLodSyncPair = std::pair<KisStrokeStrategy*, PkList<KisStrokeJobData*>>;
using KisLodSyncStrokeStrategyFactory = std::function<KisLodSyncPair(bool /*forgettable*/)>;

using KisSuspendResumePair = std::pair<KisStrokeStrategy*, PkList<KisStrokeJobData*>>;
using KisSuspendResumeStrategyFactory = std::function<KisSuspendResumePair()>;
using KisSuspendResumeStrategyPairFactory = std::function<std::pair<KisSuspendResumePair, KisSuspendResumePair>()>;


#endif /* __KIS_STROKE_STRATEGY_FACTORY_H */
