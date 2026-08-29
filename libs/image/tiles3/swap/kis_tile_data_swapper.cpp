/*
 *  SPDX-FileCopyrightText: 2010 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <cstdint>

#include <PkSemaphore.h>
#include <PkMutex.h>
#include <PkAtomic.h>
#include <PkList.h>

#include "tiles3/swap/kis_tile_data_swapper.h"
#include "tiles3/swap/kis_tile_data_swapper_p.h"
#include "tiles3/kis_tile_data.h"
#include "tiles3/kis_tile_data_store.h"
#include "tiles3/kis_tile_data_store_iterators.h"
#include "kis_debug.h"

#define SEC 1000

const std::int32_t KisTileDataSwapper::TIMEOUT = -1;
const std::int32_t KisTileDataSwapper::DELAY = 0.7 * SEC;

//#define DEBUG_SWAPPER

#ifdef DEBUG_SWAPPER
#define DEBUG_ACTION(action) dbgKrita << action
#define DEBUG_VALUE(value) dbgKrita << "\t" << ppVar(value)
#else
#define DEBUG_ACTION(action)
#define DEBUG_VALUE(value)
#endif

class SoftSwapStrategy;
class AggressiveSwapStrategy;


struct KisTileDataSwapper::Private
{
public:
    PkSemaphore semaphore;
    PkAtomicInt shouldExitFlag;
    KisTileDataStore *store;
    KisStoreLimits limits;
    PkMutex cycleLock;
};

KisTileDataSwapper::KisTileDataSwapper(KisTileDataStore *store)
    : m_d(new Private())
{
    m_d->shouldExitFlag = 0;
    m_d->store = store;
}

KisTileDataSwapper::~KisTileDataSwapper()
{
    terminateSwapper();

    if (m_thread.joinable()) {
        m_thread.join();
    }

    delete m_d;
}

void KisTileDataSwapper::start()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    if (m_running) {
        return;
    }

    if (m_thread.joinable()) {
        m_thread.join();
    }

    m_d->shouldExitFlag = false;
    m_running = true;
    m_thread = std::thread(&KisTileDataSwapper::run, this);
}

void KisTileDataSwapper::kick()
{
    m_d->semaphore.release();
}

void KisTileDataSwapper::terminateSwapper()
{
    std::unique_lock<std::mutex> lock(m_stateMutex);
    m_d->shouldExitFlag = true;

    while (m_running) {
        kick();
        m_stateCond.wait_for(lock, std::chrono::milliseconds(100));
    }
}

void KisTileDataSwapper::waitForWork()
{
    m_d->semaphore.tryAcquire(1, TIMEOUT);
}

void KisTileDataSwapper::run()
{
    while (1) {
        waitForWork();

        if (m_d->shouldExitFlag)
            break;

        std::this_thread::sleep_for(std::chrono::milliseconds(DELAY));

        doJob();
    }

    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_running = false;
    }
    m_stateCond.notify_all();
}

void KisTileDataSwapper::checkFreeMemory()
{
//    dbgKrita <<"check memory: high limit -" << m_d->limits.emergencyThreshold() <<"in mem -" << m_d->store->numTilesInMemory();
    if(m_d->store->memoryMetric() > m_d->limits.emergencyThreshold())
        doJob();
}

void KisTileDataSwapper::doJob()
{
    /**
     * In emergency case usual threads have access
     * to this function as well
     */
    PkMutexLocker locker(&m_d->cycleLock);

    std::int32_t memoryMetric = m_d->store->memoryMetric();

    DEBUG_ACTION("Started swap cycle");
    DEBUG_VALUE(m_d->store->numTiles());
    DEBUG_VALUE(m_d->store->numTilesInMemory());
    DEBUG_VALUE(memoryMetric);

    DEBUG_VALUE(m_d->limits.softLimitThreshold());
    DEBUG_VALUE(m_d->limits.hardLimitThreshold());


    if(memoryMetric > m_d->limits.softLimitThreshold()) {
        std::int32_t softFree =  memoryMetric - m_d->limits.softLimit();
        DEBUG_VALUE(softFree);
        DEBUG_ACTION("\t pass0");
        memoryMetric -= pass<SoftSwapStrategy>(softFree);
        DEBUG_VALUE(memoryMetric);

        if(memoryMetric > m_d->limits.hardLimitThreshold()) {
            std::int32_t hardFree =  memoryMetric - m_d->limits.hardLimit();
            DEBUG_VALUE(hardFree);
            DEBUG_ACTION("\t pass1");
            memoryMetric -= pass<AggressiveSwapStrategy>(hardFree);
            DEBUG_VALUE(memoryMetric);
        }
    }
}


class SoftSwapStrategy
{
public:
    typedef KisTileDataStoreIterator iterator;

    static inline iterator* beginIteration(KisTileDataStore *store) {
        return store->beginIteration();
    }

    static inline void endIteration(KisTileDataStore *store, iterator *iter) {
        store->endIteration(iter);
    }

    static inline bool isInteresting(KisTileData *td) {
        // We are working with mementoed tiles only...
        return td->historical();
    }

    static inline bool swapOutFirst(KisTileData *td) {
        return td->age() > 0;
    }
};

class AggressiveSwapStrategy
{
public:
    typedef KisTileDataStoreClockIterator iterator;

    static inline iterator* beginIteration(KisTileDataStore *store) {
        return store->beginClockIteration();
    }

    static inline void endIteration(KisTileDataStore *store, iterator *iter) {
        store->endIteration(iter);
    }

    static inline bool isInteresting(KisTileData *td) {
        // Add some aggression...
        (void)(td);
        return true; // >:)
    }

    static inline bool swapOutFirst(KisTileData *td) {
        return td->age() > 0;
    }
};


template<class strategy>
std::int64_t KisTileDataSwapper::pass(std::int64_t needToFreeMetric)
{
    std::int64_t freedMetric = 0;
    PkList<KisTileData*> additionalCandidates;

    typename strategy::iterator *iter =
        strategy::beginIteration(m_d->store);

    KisTileData *item = 0;

    while (iter->hasNext()) {
        item = iter->next();

        if (freedMetric >= needToFreeMetric) break;

        if (!strategy::isInteresting(item)) continue;

        if (strategy::swapOutFirst(item)) {
            if (iter->trySwapOut(item)) {
                freedMetric += item->pixelSize();
            }
        }
        else {
            item->markOld();
            additionalCandidates.append(item);
        }

    }

    for (KisTileData *item : additionalCandidates) {
        if (freedMetric >= needToFreeMetric) break;

        if (iter->trySwapOut(item)) {
            freedMetric += item->pixelSize();
        }
    }

    strategy::endIteration(m_d->store, iter);

    return freedMetric;
}

void KisTileDataSwapper::testingRereadConfig()
{
    m_d->limits = KisStoreLimits();
}
