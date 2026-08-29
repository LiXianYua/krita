/*
 *  SPDX-FileCopyrightText: 2009 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_TILE_DATA_POOLER_H_
#define KIS_TILE_DATA_POOLER_H_

#include <cstdint>

#include <thread>
#include <mutex>
#include <condition_variable>

#include <PkSemaphore.h>
#include <PkAtomic.h>
#include <PkList.h>

#include "kritaimage_export.h"

class KisTileDataStore;
class KisTileData;


class KRITAIMAGE_EXPORT KisTileDataPooler
{
public:

    KisTileDataPooler(KisTileDataStore *store, std::int32_t memoryLimit = -1);
    ~KisTileDataPooler();

    void start();
    void kick();
    void terminatePooler();

    void testingRereadConfig();

    std::int64_t lastPoolMemoryMetric() const;
    std::int64_t lastRealMemoryMetric() const;
    std::int64_t lastHistoricalMemoryMetric() const;


    /**
     * Is case the pooler thread is not running, the user might force
     * recalculation of the memory statistics explicitly.
     */
    void forceUpdateMemoryStats();

    bool isRunning() const;

protected:
    static const std::int32_t MAX_NUM_CLONES;
    static const std::int32_t MAX_TIMEOUT;
    static const std::int32_t MIN_TIMEOUT;
    static const std::int32_t TIMEOUT_FACTOR;

    void waitForWork();
    std::int32_t numClonesNeeded(KisTileData *td) const;
    void cloneTileData(KisTileData *td, std::int32_t numClones) const;
    void run();

    inline int clonesMetric(KisTileData *td, int numClones);
    inline int clonesMetric(KisTileData *td);

    inline void tryFreeOrphanedClones(KisTileData *td);
    inline std::int32_t needMemory(KisTileData *td);
    inline std::int32_t canDonorMemory(KisTileData *td);
    std::int32_t tryGetMemory(PkList<KisTileData*> &donors, std::int32_t memoryMetric);

    template<class Iter>
        void getLists(Iter *iter, PkList<KisTileData*> &beggars,
                      PkList<KisTileData*> &donors,
                      std::int32_t &memoryOccupied,
                      std::int32_t &statRealMemory,
                      std::int32_t &statHistoricalMemory);

    bool processLists(PkList<KisTileData*> &beggars,
                      PkList<KisTileData*> &donors,
                      std::int32_t &memoryOccupied);

private:
    void debugTileStatistics();
protected:
    PkSemaphore m_semaphore;
    PkAtomicInt m_shouldExitFlag;
    KisTileDataStore *m_store;
    std::int32_t m_timeout;
    bool m_lastCycleHadWork;
    std::int32_t m_memoryLimit;
    std::int32_t m_lastPoolMemoryMetric;
    std::int32_t m_lastRealMemoryMetric;
    std::int32_t m_lastHistoricalMemoryMetric;

    std::thread m_thread;
    mutable std::mutex m_stateMutex;
    std::condition_variable m_stateCond;
    bool m_running = false;
};



#endif /* KIS_TILE_DATA_POOLER_H_ */
