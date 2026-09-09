/*
 *  SPDX-FileCopyrightText: 2011 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_tile_data_pooler_test.h"
#include <simpletest.h>

#include "tiles3/kis_tiled_data_manager.h"

#include "tiles3/kis_tile_data_store.h"
#include "tiles3/kis_tile_data_store_iterators.h"

#include "tiles3/kis_tile_data_pooler.h"

#include <chrono>
#include <future>
#include <thread>

#ifdef DEBUG_TILES
#define PRETTY_TILE(idx, td)                                    \
    qDebug << "tile" << i                                     \
           << "\tusers" << td->numUsers()                     \
           << "\tclones" << td->m_clonesStack.size()          \
           << "\tage" << td->age();

#else
#define PRETTY_TILE(idx, td)
#endif

void KisTileDataPoolerTest::testCycles()
{
    const qint32 pixelSize = 1;
    quint8 defaultPixel = 128;

    KisTileDataStore::instance()->debugClear();

    for(int i = 0; i < 12; i++) {
        KisTileData *td =
            KisTileDataStore::instance()->createDefaultTileData(pixelSize, &defaultPixel);

        for(int j = 0; j < 1 + (2 - i % 3); j++) {
            td->acquire();
        }

        if(!(i/6)) {
            td->markOld();
        }

        if(!((i / 3) & 1)) {
            td->m_clonesStack.push(new KisTileData(*td));
        }

        PRETTY_TILE(i, td);
    }

    {
        KisTileDataPooler pooler(KisTileDataStore::instance(), 5);
        pooler.start();
        pooler.kick();
        pooler.kick();

        QTest::qSleep(500);

        pooler.terminatePooler();
    }

    int i = 0;
    KisTileData *item;
    KisTileDataStoreIterator *iter =
        KisTileDataStore::instance()->beginIteration();

    while(iter->hasNext()) {
        item = iter->next();

        int expectedClones;

        switch(i) {
        case 6:
        case 7:
        case 10:
            expectedClones = 1;
            break;
        case 9:
            expectedClones = 2;
            break;
        default:
            expectedClones = 0;
        }

        PRETTY_TILE(i, item);
        if (item->m_clonesStack.size() != expectedClones) {
            qDebug() << ppVar(item->m_clonesStack.size()) << ppVar(expectedClones);
            QEXPECT_FAIL("", "The clonesStack's size is not as expected", Continue);
        }
        QCOMPARE(item->m_clonesStack.size(), expectedClones);
        i++;
    }


    KisTileDataStore::instance()->endIteration(iter);
    KisTileDataStore::instance()->debugClear();
}

void KisTileDataPoolerTest::testRemovalWaitsForPoolRelease()
{
    using namespace std::chrono_literals;

    KisTileDataStore *store = KisTileDataStore::instance();
    store->testingSuspendPooler();
    store->debugClear();

    const std::int32_t pixelSize = 4;
    const std::uint8_t defaultPixel[] = {128, 128, 128, 255};
    KisTileData *tileData = store->createDefaultTileData(pixelSize, defaultPixel);

    std::promise<void> removalStartedPromise;
    std::future<void> removalStarted = removalStartedPromise.get_future();
    std::promise<void> removalDonePromise;
    std::future<void> removalDone = removalDonePromise.get_future();
    std::promise<void> purgeStartedPromise;
    std::future<void> purgeStarted = purgeStartedPromise.get_future();
    std::promise<void> purgeDonePromise;
    std::future<void> purgeDone = purgeDonePromise.get_future();

    KisTileData::poolReleaseMutex().lock();
    std::thread remover([&] {
        removalStartedPromise.set_value();
        store->freeTileData(tileData);
        removalDonePromise.set_value();
    });
    std::thread purger([&] {
        purgeStartedPromise.set_value();
        KisTileData::releaseInternalPools();
        purgeDonePromise.set_value();
    });

    removalStarted.wait();
    purgeStarted.wait();
    const bool removalCompletedWhilePoolReleaseWasBlocked =
        removalDone.wait_for(500ms) == std::future_status::ready;
    const bool purgeCompletedWhilePoolReleaseWasBlocked =
        purgeDone.wait_for(500ms) == std::future_status::ready;
    const std::int32_t visibleTilesWhileBlocked = store->numTilesInMemory();

    KisTileData::poolReleaseMutex().unlock();
    remover.join();
    purger.join();

    QVERIFY(!removalCompletedWhilePoolReleaseWasBlocked);
    QVERIFY(!purgeCompletedWhilePoolReleaseWasBlocked);
    QCOMPARE(visibleTilesWhileBlocked, 1);
    QCOMPARE(store->numTilesInMemory(), 0);

    store->testingResumePooler();
}

void KisTileDataPoolerTest::testAcquireCloneCleanupWaitsForPoolRelease()
{
    using namespace std::chrono_literals;

    KisTileDataStore *store = KisTileDataStore::instance();
    store->testingSuspendPooler();
    store->debugClear();

    const std::int32_t pixelSize = 4;
    const std::uint8_t defaultPixel[] = {128, 128, 128, 255};
    KisTileData *tileData = store->createDefaultTileData(pixelSize, defaultPixel);
    tileData->acquire();
    tileData->m_clonesStack.push(new KisTileData(*tileData, false));

    std::promise<void> acquireStartedPromise;
    std::future<void> acquireStarted = acquireStartedPromise.get_future();
    std::promise<void> acquireDonePromise;
    std::future<void> acquireDone = acquireDonePromise.get_future();

    KisTileData::poolReleaseMutex().lock();
    std::thread acquirer([&] {
        acquireStartedPromise.set_value();
        tileData->acquire();
        acquireDonePromise.set_value();
    });

    acquireStarted.wait();
    const bool acquireCompletedWhilePoolReleaseWasBlocked =
        acquireDone.wait_for(500ms) == std::future_status::ready;
    const std::int32_t clonesWhileBlocked = tileData->m_clonesStack.size();

    KisTileData::poolReleaseMutex().unlock();
    acquirer.join();

    QVERIFY(!acquireCompletedWhilePoolReleaseWasBlocked);
    QCOMPARE(clonesWhileBlocked, 1);
    QCOMPARE(tileData->m_clonesStack.size(), 0);

    tileData->release();
    tileData->release();
    store->testingResumePooler();
}

SIMPLE_TEST_MAIN(KisTileDataPoolerTest)
