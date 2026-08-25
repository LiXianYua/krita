/*
 *  SPDX-FileCopyrightText: 2010 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_SIMPLE_UPDATE_QUEUE_H
#define __KIS_SIMPLE_UPDATE_QUEUE_H

#include <PkMutex.h>
#include "kis_updater_context.h"
#include <KisProjectionUpdateFlags.h>

typedef PkList<KisBaseRectsWalkerSP> KisWalkersList;
typedef PkListIterator<KisBaseRectsWalkerSP> KisWalkersListIterator;
typedef PkMutableListIterator<KisBaseRectsWalkerSP> KisMutableWalkersListIterator;

typedef PkList<KisSpontaneousJob*> KisSpontaneousJobsList;
typedef PkListIterator<KisSpontaneousJob*> KisSpontaneousJobsListIterator;
typedef PkMutableListIterator<KisSpontaneousJob*> KisMutableSpontaneousJobsListIterator;


class KRITAIMAGE_EXPORT KisSimpleUpdateQueue
{
public:
    KisSimpleUpdateQueue();
    virtual ~KisSimpleUpdateQueue();

    void processQueue(KisUpdaterContext &updaterContext);

    void addUpdateJob(KisNodeSP node, const PkVector<PkRect> &rects, const PkRect& cropRect, int levelOfDetail, KisProjectionUpdateFlags flags);
    void addFullRefreshJob(KisNodeSP node, const PkVector<PkRect> &rects, const PkRect& cropRect, int levelOfDetail, KisProjectionUpdateFlags flags);

    // simplified overload for testing purposes only
    void addUpdateJob(KisNodeSP node, const PkRect &rc, const PkRect& cropRect, int levelOfDetail);

    // simplified overload for testing purposes only
    void addFullRefreshJob(KisNodeSP node, const PkRect &rc, const PkRect& cropRect, int levelOfDetail);

    void addSpontaneousJob(KisSpontaneousJob *spontaneousJob);


    void optimize();

    /**
     * Returns true if the update queue is empty, i.e. there are
     * no update or spontaneous jobs pending
     */
    bool isEmpty() const;

    /**
     * Works in the same way as isEmpty(), except that it will not
     * wait on the mutex in case there is any contestion on the queue.
     *
     * If some other threads are contending on the queue, it will just
     * return `false`, whatever the state of the queue is.
     */
    bool isIdle() const;

    qint32 sizeMetric() const;

    void updateSettings();

    int overrideLevelOfDetail() const;

protected:
    void addJob(KisNodeSP node, const PkVector<PkRect> &rects, const PkRect& cropRect, int levelOfDetail, KisBaseRectsWalker::UpdateType type, bool dontInvalidateFrames);

    bool processOneJob(KisUpdaterContext &updaterContext);

    bool trySplitJob(KisNodeSP node, const PkRect& rc, const PkRect& cropRect, int levelOfDetail, KisBaseRectsWalker::UpdateType type, bool dontInvalidateFrames);
    bool tryMergeJob(KisNodeSP node, const PkRect& rc, const PkRect& cropRect, int levelOfDetail, KisBaseRectsWalker::UpdateType type, bool dontInvalidateFrames);

    void collectJobs(KisBaseRectsWalkerSP &baseWalker, PkRect baseRect,
                     const qreal maxAlpha);
    bool joinRects(PkRect& baseRect, const PkRect& newRect, qreal maxAlpha);

protected:

    mutable PkMutex m_lock;
    KisWalkersList m_updatesList;
    KisSpontaneousJobsList m_spontaneousJobsList;

    /**
     * Parameters of optimization
     * (loaded from a configuration file)
     */

    /**
     * Big update areas are split into a set of smaller
     * ones, m_patchWidth and m_patchHeight represent the
     * size of these areas.
     */
    qint32 m_patchWidth;
    qint32 m_patchHeight;

    /**
     * Maximum coefficient of work while regular optimization()
     */
    qreal m_maxCollectAlpha;

    /**
     * Maximum coefficient of work when to rects are considered
     * similar and are merged in tryMergeJob()
     */
    qreal m_maxMergeAlpha;

    /**
     * The coefficient of work used while collecting phase of tryToMerge()
     */
    qreal m_maxMergeCollectAlpha;

    int m_overrideLevelOfDetail;
};

class KRITAIMAGE_EXPORT KisTestableSimpleUpdateQueue : public KisSimpleUpdateQueue
{
public:
    KisWalkersList& getWalkersList();
    KisSpontaneousJobsList& getSpontaneousJobsList();
};

#endif /* __KIS_SIMPLE_UPDATE_QUEUE_H */

