/*
 *  SPDX-FileCopyrightText: 2009 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <cstdint>


#include <stdio.h>
#include "kis_tile_data.h"
#include "kis_tile_data_store.h"
#include "kis_tile_data_store_iterators.h"
#include "kis_debug.h"
#include "kis_tile_data_pooler.h"
#include "kis_image_config.h"


const std::int32_t KisTileDataPooler::MAX_NUM_CLONES = 16;
const std::int32_t KisTileDataPooler::MAX_TIMEOUT = 60000; // 01m00s
const std::int32_t KisTileDataPooler::MIN_TIMEOUT = 100; // 00m00.100s
const std::int32_t KisTileDataPooler::TIMEOUT_FACTOR = 2;

//#define DEBUG_POOLER

#ifdef DEBUG_POOLER
#define DEBUG_CLONE_ACTION(td, numClones)                               \
    printf("Cloned (%d):\t\t\t\t0x%X (clones: %d, users: %d, refs: %d)\n", \
           numClones, td, td->m_clonesStack.size(),                      \
           (int)td->m_usersCount, (int)td->m_refCount)
#define DEBUG_SIMPLE_ACTION(action)     \
    printf("pooler: %s\n", action)

#define RUNTIME_SANITY_CHECK(td) do {                                   \
        if(td->m_usersCount < td->m_refCount) {                         \
            infoTiles << "**** Suspicious tiledata:" << td             \
                      << "(clones:" << td->m_clonesStack.size()         \
                      << ", users:" << (int)td->m_usersCount            \
                      << ", refs:" << (int)td->m_refCount << ") ****"; \
        }                                                               \
        if(td->m_usersCount <= 0) {                                     \
            qFatal("pooler: Tiledata 0x%X has zero users counter. Crashing...", td); \
        }                                                               \
    } while(0)                                                          \

#define DEBUG_TILE_STATISTICS() debugTileStatistics()

#define DEBUG_LISTS(mem, beggars, beggarsMem, donors, donorsMem)        \
    do {                                                                \
    dbgKrita << "--- getLists finished ---";                            \
    dbgKrita << "  memoryOccupied:" << mem << "/" << m_memoryLimit;     \
    dbgKrita << "  donors:" << donors.size()                            \
             << "(mem:" << donorsMem << ")";                            \
    dbgKrita << "  beggars:" << beggars.size()                          \
             << "(mem:" << beggarsMem << ")";                           \
    dbgKrita << "--- ----------------- ---";                            \
    } while(0)

#define DEBUG_ALLOC_CLONE(mem, totalMem)                                \
        dbgKrita << "Alloc mem for clones:" << mem                      \
                 << "\tMem usage:" << totalMem << "/" << m_memoryLimit

#define DEBUG_FREE_CLONE(freed, demanded)                               \
            dbgKrita << "Freed mem for clones:" << freed                \
                     << "/" << qAbs(demanded)

#else
#define DEBUG_CLONE_ACTION(td, numClones)
#define DEBUG_SIMPLE_ACTION(action)
#define RUNTIME_SANITY_CHECK(td)
#define DEBUG_TILE_STATISTICS()
#define DEBUG_LISTS(mem, beggars, beggarsMem, donors, donorsMem)               \
    (void)(mem);                                                             \
    (void)(beggars);                                                         \
    (void)(beggarsMem);                                                      \
    (void)(donors);                                                          \
    (void)(donorsMem);
#define DEBUG_ALLOC_CLONE(mem, totalMem)
#define DEBUG_FREE_CLONE(freed, demanded)
#endif


KisTileDataPooler::KisTileDataPooler(KisTileDataStore *store, std::int32_t memoryLimit)
{
    m_shouldExitFlag = 0;
    m_store = store;
    m_timeout = MIN_TIMEOUT;
    m_lastCycleHadWork = false;
    m_lastPoolMemoryMetric = 0;
    m_lastRealMemoryMetric = 0;
    m_lastHistoricalMemoryMetric = 0;

    if(memoryLimit >= 0) {
        m_memoryLimit = memoryLimit;
    }
    else {
        m_memoryLimit = MiB_TO_METRIC(KisImageConfig(true).poolLimit());
    }
}

KisTileDataPooler::~KisTileDataPooler()
{
    terminatePooler();

    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void KisTileDataPooler::start()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    if (m_running) {
        return;
    }

    if (m_thread.joinable()) {
        m_thread.join();
    }

    m_running = true;
    m_thread = std::thread(&KisTileDataPooler::run, this);
}

bool KisTileDataPooler::isRunning() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_running;
}

void KisTileDataPooler::kick()
{
    m_semaphore.release();
}

void KisTileDataPooler::terminatePooler()
{
    std::unique_lock<std::mutex> lock(m_stateMutex);
    m_shouldExitFlag = true;

    while (m_running) {
        kick();
        m_stateCond.wait_for(lock, std::chrono::milliseconds(100));
    }
}

std::int32_t KisTileDataPooler::numClonesNeeded(KisTileData *td) const
{
    RUNTIME_SANITY_CHECK(td);
    std::int32_t numUsers = td->m_usersCount;
    std::int32_t numPresentClones = td->m_clonesStack.size();
    std::int32_t totalClones = qMin(numUsers - 1, MAX_NUM_CLONES);

    return totalClones - numPresentClones;
}

void KisTileDataPooler::cloneTileData(KisTileData *td, std::int32_t numClones) const
{
    if (numClones > 0) {
        td->blockSwapping();
        for (std::int32_t i = 0; i < numClones; i++) {
            td->m_clonesStack.push(new KisTileData(*td, false));
        }
        td->unblockSwapping();
    } else {
        std::int32_t numUnneededClones = qAbs(numClones);
        for (std::int32_t i = 0; i < numUnneededClones; i++) {
            KisTileData *clone = 0;

            bool result = td->m_clonesStack.pop(clone);
            if(!result) break;

            delete clone;
        }
    }

    DEBUG_CLONE_ACTION(td, numClones);
}

void KisTileDataPooler::waitForWork()
{
    bool success;

    if (m_lastCycleHadWork)
        success = m_semaphore.tryAcquire(1, m_timeout);
    else {
        m_semaphore.acquire();
        success = true;
    }

    m_lastCycleHadWork = false;
    if (success) {
        m_timeout = MIN_TIMEOUT;
    } else {
        m_timeout *= TIMEOUT_FACTOR;
        m_timeout = qMin(m_timeout, MAX_TIMEOUT);
    }
}

void KisTileDataPooler::run()
{
    if(!m_memoryLimit) {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_running = false;
        m_stateCond.notify_all();
        return;
    }

    m_shouldExitFlag = false;

    while (1) {
        DEBUG_SIMPLE_ACTION("went to bed... Zzz...");

        waitForWork();

        if (m_shouldExitFlag)
            break;

        std::this_thread::yield();
        DEBUG_SIMPLE_ACTION("cycle started");


        KisTileDataStoreReverseIterator *iter = m_store->beginReverseIteration();
        PkList<KisTileData*> beggars;
        PkList<KisTileData*> donors;
        std::int32_t memoryOccupied;

        std::int32_t statRealMemory;
        std::int32_t statHistoricalMemory;


        getLists(iter, beggars, donors,
                 memoryOccupied,
                 statRealMemory,
                 statHistoricalMemory);

        m_lastCycleHadWork =
            processLists(beggars, donors, memoryOccupied);

        m_lastPoolMemoryMetric = memoryOccupied;
        m_lastRealMemoryMetric = statRealMemory;
        m_lastHistoricalMemoryMetric = statHistoricalMemory;

        m_store->endIteration(iter);

        DEBUG_TILE_STATISTICS();
        DEBUG_SIMPLE_ACTION("cycle finished");
    }

    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_running = false;
    }
    m_stateCond.notify_all();
}

void KisTileDataPooler::forceUpdateMemoryStats()
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(!isRunning());

    KisTileDataStoreReverseIterator *iter = m_store->beginReverseIteration();
    PkList<KisTileData*> beggars;
    PkList<KisTileData*> donors;
    std::int32_t memoryOccupied;

    std::int32_t statRealMemory;
    std::int32_t statHistoricalMemory;


    getLists(iter, beggars, donors,
             memoryOccupied,
             statRealMemory,
             statHistoricalMemory);

    m_lastPoolMemoryMetric = memoryOccupied;
    m_lastRealMemoryMetric = statRealMemory;
    m_lastHistoricalMemoryMetric = statHistoricalMemory;

    m_store->endIteration(iter);
}

long long KisTileDataPooler::lastPoolMemoryMetric() const
{
    return m_lastPoolMemoryMetric;
}

long long KisTileDataPooler::lastRealMemoryMetric() const
{
    return m_lastRealMemoryMetric;
}

long long KisTileDataPooler::lastHistoricalMemoryMetric() const
{
    return m_lastHistoricalMemoryMetric;
}

inline int KisTileDataPooler::clonesMetric(KisTileData *td, int numClones) {
    return numClones * td->pixelSize();
}

inline int KisTileDataPooler::clonesMetric(KisTileData *td) {
    return td->m_clonesStack.size() * td->pixelSize();
}

inline void KisTileDataPooler::tryFreeOrphanedClones(KisTileData *td)
{
    std::int32_t extraClones = -numClonesNeeded(td);

    if(extraClones > 0) {
        cloneTileData(td, -extraClones);
    }
}

inline std::int32_t KisTileDataPooler::needMemory(KisTileData *td)
{
    std::int32_t clonesNeeded = !td->age() ? qMax(0, numClonesNeeded(td)) : 0;
    return clonesMetric(td, clonesNeeded);
}

inline std::int32_t KisTileDataPooler::canDonorMemory(KisTileData *td)
{
    return td->age() && clonesMetric(td);
}

template<class Iter>
void KisTileDataPooler::getLists(Iter *iter,
                                 PkList<KisTileData*> &beggars,
                                 PkList<KisTileData*> &donors,
                                 std::int32_t &memoryOccupied,
                                 std::int32_t &statRealMemory,
                                 std::int32_t &statHistoricalMemory)
{
    memoryOccupied = 0;
    statRealMemory = 0;
    statHistoricalMemory = 0;

    std::int32_t needMemoryTotal = 0;
    std::int32_t canDonorMemoryTotal = 0;

    std::int32_t neededMemory;
    std::int32_t donoredMemory;

    KisTileData *item;

    while(iter->hasNext()) {
        item = iter->next();

        tryFreeOrphanedClones(item);

        if((neededMemory = needMemory(item))) {
            needMemoryTotal += neededMemory;
            beggars.append(item);
        }
        else if((donoredMemory = canDonorMemory(item))) {
            canDonorMemoryTotal += donoredMemory;
            donors.append(item);
        }

        memoryOccupied += clonesMetric(item);

        // statistics gathering
        if (item->historical()) {
            statHistoricalMemory += item->pixelSize();
        } else {
            statRealMemory += item->pixelSize();
        }
    }

    DEBUG_LISTS(memoryOccupied,
                beggars, needMemoryTotal,
                donors, canDonorMemoryTotal);
}

std::int32_t KisTileDataPooler::tryGetMemory(PkList<KisTileData*> &donors,
                                       std::int32_t memoryMetric)
{
    std::int32_t memoryFreed = 0;

    for (int i = donors.size() - 1; i >= 0 && memoryFreed < memoryMetric; --i) {
        KisTileData *item = donors[i];

        std::int32_t numClones = item->m_clonesStack.size();
        cloneTileData(item, -numClones);
        memoryFreed += clonesMetric(item, numClones);

        donors.removeAt(i);
    }

    return memoryFreed;
}

bool KisTileDataPooler::processLists(PkList<KisTileData*> &beggars,
                                     PkList<KisTileData*> &donors,
                                     std::int32_t &memoryOccupied)
{
    bool hadWork = false;


    for (KisTileData *item : beggars) {
        std::int32_t clonesNeeded = numClonesNeeded(item);
        std::int32_t clonesMemory = clonesMetric(item, clonesNeeded);

        std::int32_t memoryLeft =
            m_memoryLimit - (memoryOccupied + clonesMemory);

        if(memoryLeft < 0) {
            std::int32_t freedMemory = tryGetMemory(donors, -memoryLeft);
            memoryOccupied -= freedMemory;

            DEBUG_FREE_CLONE(freedMemory, memoryLeft);

            if(m_memoryLimit < memoryOccupied + clonesMemory)
                break;
        }

        cloneTileData(item, clonesNeeded);
        DEBUG_ALLOC_CLONE(clonesMemory, memoryOccupied);

        memoryOccupied += clonesMemory;
        hadWork = true;
    }

    return hadWork;
}

void KisTileDataPooler::debugTileStatistics()
{
    /**
     * Assume we are called from the inside of the loop.
     * This means m_store is already locked
     */

    long long preallocatedTiles=0;

    KisTileDataStoreIterator *iter = m_store->beginIteration();
    KisTileData *item;

    while(iter->hasNext()) {
        item = iter->next();
        preallocatedTiles += item->m_clonesStack.size();
    }

    m_store->endIteration(iter);

    dbgKrita << "Tiles statistics:\t total:" << m_store->numTiles() << "\t preallocated:"<< preallocatedTiles;
}

void KisTileDataPooler::testingRereadConfig()
{
    m_memoryLimit = MiB_TO_METRIC(KisImageConfig(true).poolLimit());
}
