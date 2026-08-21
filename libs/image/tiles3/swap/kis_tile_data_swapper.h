/*
 *  SPDX-FileCopyrightText: 2010 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_TILE_DATA_SWAPPER_H_
#define KIS_TILE_DATA_SWAPPER_H_

#include <thread>
#include <mutex>
#include <condition_variable>

#include "kritaimage_export.h"


class KisTileDataStore;
class KisTileData;

class KRITAIMAGE_EXPORT KisTileDataSwapper
{
public:

    KisTileDataSwapper(KisTileDataStore *store);
    ~KisTileDataSwapper();

    void start();
    void kick();
    void terminateSwapper();
    void checkFreeMemory();
    bool isRunning() const;

    void testingRereadConfig();

private:
    void waitForWork();
    void run();

    void doJob();
    template<class strategy> qint64 pass(qint64 needToFreeMetric);

private:
    static const qint32 TIMEOUT;
    static const qint32 DELAY;

private:
    struct Private;
    Private * const m_d;

    std::thread m_thread;
    std::mutex m_stateMutex;
    std::condition_variable m_stateCond;
    bool m_running = false;
};



#endif /* KIS_TILE_DATA_SWAPPER_H_ */
