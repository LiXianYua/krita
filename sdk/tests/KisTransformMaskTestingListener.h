/*
 *  SPDX-FileCopyrightText: 2024 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KISTRANSFORMMASKTESTINGLISTENER_H
#define KISTRANSFORMMASKTESTINGLISTENER_H

#include <boost/operators.hpp>

#include <kis_debug.h>
#include <PkMutex.h>

#include <KisTransformMaskTestingInterface.h>


struct KisTransformMaskTestingListener : KisTransformMaskTestingInterface
{
    struct Data : boost::equality_comparable<Data>
    {
        int forceUpdateTimedNode = 0;
        int threadSafeForceStaticImageUpdate = 0;
        int slotDelayedStaticImageUpdate = 0;
        int decorateRectTriggeredStaticImageUpdate = 0;
        int recalculateStaticImage = 0;

        bool operator==(const Data &rhs) const {
            return
                rhs.forceUpdateTimedNode == forceUpdateTimedNode &&
                rhs.threadSafeForceStaticImageUpdate == threadSafeForceStaticImageUpdate &&
                rhs.slotDelayedStaticImageUpdate == slotDelayedStaticImageUpdate &&
                bool(rhs.decorateRectTriggeredStaticImageUpdate > 0) ==
                bool(decorateRectTriggeredStaticImageUpdate > 0) &&
                rhs.recalculateStaticImage == recalculateStaticImage;
        }
    };

    void notifyForceUpdateTimedNode() override {
        PkMutexLocker l(&m_mutex);
        m_data.forceUpdateTimedNode++;
    }
    void notifyThreadSafeForceStaticImageUpdate() override {
        PkMutexLocker l(&m_mutex);
        m_data.threadSafeForceStaticImageUpdate++;
    }
    void notifySlotDelayedStaticUpdate() override {
        PkMutexLocker l(&m_mutex);
        m_data.slotDelayedStaticImageUpdate++;
    }
    void notifyDecorateRectTriggeredStaticImageUpdate() override {
        PkMutexLocker l(&m_mutex);
        m_data.decorateRectTriggeredStaticImageUpdate++;
    }
    void notifyRecalculateStaticImage() override {
        PkMutexLocker l(&m_mutex);
        m_data.recalculateStaticImage++;
    }

    Data stats() const {
        PkMutexLocker l(&m_mutex);
        return m_data;
    }
    void clear() {
        PkMutexLocker l(&m_mutex);
        m_data = Data();
    }

private:
    mutable PkMutex m_mutex;
    Data m_data;
};

inline PkDebug operator<<(PkDebug dbg, const KisTransformMaskTestingListener::Data &d)
{
    dbg.nospace() << "("
                  << ppVar(d.decorateRectTriggeredStaticImageUpdate) << " "
                  << ppVar(d.slotDelayedStaticImageUpdate) << " "
                  << ppVar(d.forceUpdateTimedNode) << " "
                  << ppVar(d.threadSafeForceStaticImageUpdate) << " "
                  << ppVar(d.recalculateStaticImage)
                  << " )";

    return dbg.space();
}


#endif // KISTRANSFORMMASKTESTINGLISTENER_H
