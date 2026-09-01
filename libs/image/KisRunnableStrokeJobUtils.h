/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISRUNNABLESTROKEJOBUTILS_H
#define KISRUNNABLESTROKEJOBUTILS_H

#include <PkContainerAlgo.h>
#include <PkVector.h>

#include "kis_stroke_job_strategy.h"
#include "KisRunnableStrokeJobData.h"

namespace KritaUtils
{

template <typename Func, typename Job>
void addJobSequential(PkVector<Job*> &jobs, Func func) {
    jobs.append(new KisRunnableStrokeJobData(func, KisStrokeJobData::SEQUENTIAL));
}

template <typename Func, typename Job>
void addJobSequentialExclusive(PkVector<Job*> &jobs, Func func) {
    jobs.append(new KisRunnableStrokeJobData(func, KisStrokeJobData::SEQUENTIAL, KisStrokeJobData::EXCLUSIVE));
}


template <typename Func, typename Job>
void addJobConcurrent(PkVector<Job*> &jobs, Func func) {
    jobs.append(new KisRunnableStrokeJobData(func, KisStrokeJobData::CONCURRENT));
}

template <typename Func, typename Job>
void addJobBarrier(PkVector<Job*> &jobs, Func func) {
    jobs.append(new KisRunnableStrokeJobData(func, KisStrokeJobData::BARRIER));
}

template <typename Func, typename Job>
void addJobBarrierExclusive(PkVector<Job*> &jobs, Func func) {
    jobs.append(new KisRunnableStrokeJobData(func, KisStrokeJobData::BARRIER, KisStrokeJobData::EXCLUSIVE));
}

template <typename Func, typename Job>
void addJobUniquelyConcurrent(PkVector<Job*> &jobs, Func func) {
    jobs.append(new KisRunnableStrokeJobData(func, KisStrokeJobData::UNIQUELY_CONCURRENT));
}

template <typename Func, typename Job>
void addJobSequential(PkVector<Job*> &jobs, int lod, Func func) {
    Job* data = new KisRunnableStrokeJobData(func, KisStrokeJobData::SEQUENTIAL);
    data->setLevelOfDetailOverride(lod);
    jobs.append(data);
}

template <typename Func, typename Job>
void addJobSequentialExclusive(PkVector<Job*> &jobs, int lod, Func func) {
    Job* data = new KisRunnableStrokeJobData(func, KisStrokeJobData::SEQUENTIAL, KisStrokeJobData::EXCLUSIVE);
    data->setLevelOfDetailOverride(lod);
    jobs.append(data);
}

template <typename Func, typename Job>
void addJobConcurrent(PkVector<Job*> &jobs, int lod, Func func) {
    Job* data = new KisRunnableStrokeJobData(func, KisStrokeJobData::CONCURRENT);
    data->setLevelOfDetailOverride(lod);
    jobs.append(data);
}

template <typename Func, typename Job>
void addJobBarrier(PkVector<Job*> &jobs, int lod, Func func) {
    Job* data = new KisRunnableStrokeJobData(func, KisStrokeJobData::BARRIER);
    data->setLevelOfDetailOverride(lod);
    jobs.append(data);
}

template <typename Func, typename Job>
void addJobUniquelyConcurrent(PkVector<Job*> &jobs, int lod, Func func) {
    Job* data = new KisRunnableStrokeJobData(func, KisStrokeJobData::UNIQUELY_CONCURRENT);
    data->setLevelOfDetailOverride(lod);
    jobs.append(data);
}


template <typename Func, typename Job>
void addJobSequentialNoCancel(PkVector<Job*> &jobs, Func func) {
    Job* data = new KisRunnableStrokeJobData(func, KisStrokeJobData::SEQUENTIAL);
    data->setCancellable(false);
    jobs.append(data);
}

template <typename Func, typename Job>
void addJobSequentialExclusiveNoCancel(PkVector<Job*> &jobs, Func func) {
    Job* data = new KisRunnableStrokeJobData(func, KisStrokeJobData::SEQUENTIAL, KisStrokeJobData::EXCLUSIVE);
    data->setCancellable(false);
    jobs.append(data);
}


template <typename Func, typename Job>
void addJobConcurrentNoCancel(PkVector<Job*> &jobs, Func func) {
    Job* data = new KisRunnableStrokeJobData(func, KisStrokeJobData::CONCURRENT);
    data->setCancellable(false);
    jobs.append(data);
}

template <typename Func, typename Job>
void addJobBarrierNoCancel(PkVector<Job*> &jobs, Func func) {
    Job* data = new KisRunnableStrokeJobData(func, KisStrokeJobData::BARRIER);
    data->setCancellable(false);
    jobs.append(data);
}

template <typename Func, typename Job>
void addJobBarrierExclusiveNoCancel(PkVector<Job*> &jobs, Func func) {
    Job* data = new KisRunnableStrokeJobData(func, KisStrokeJobData::BARRIER, KisStrokeJobData::EXCLUSIVE);
    data->setCancellable(false);
    jobs.append(data);
}

template <typename Func, typename Job>
void addJobUniquelyConcurrentNoCancel(PkVector<Job*> &jobs, Func func) {
    Job* data = new KisRunnableStrokeJobData(func, KisStrokeJobData::UNIQUELY_CONCURRENT);
    data->setCancellable(false);
    jobs.append(data);
}

}

#endif // KISRUNNABLESTROKEJOBUTILS_H
