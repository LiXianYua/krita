/*
 *  SPDX-FileCopyrightText: 2011 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_low_memory_tests.h"

#include <simpletest.h>

#include <QThreadPool>

#include "kis_image_config.h"
#include <cstdlib>
#include <filesystem>

// R-67 ① 修复：本 target 构造 `KisImageConfig config(false)`，其析构在 `!m_readOnly`
// 时**无条件** `m_config.sync()`，把 swap/内存键写进**使用者的真实 kritarc**。
// 改前复现（受控初值 4096/64/16 → 跑完变 40/1/1，md5 0f890b86… → 7fe43df7…）：
// .superpowers/sdd/R-65/evidence/t24/r67-1-BEFORE-repro.txt
//
// 修法：把本进程的 HOME 指到一个进程私有临时目录 —— `PkConfigStore` 的落点是
// `homePath()/Library/Preferences/kritarc`（macOS；Linux 为 `$XDG_CONFIG_HOME` 或
// `$HOME/.config`），而 `PkConfigStore` 是**首次使用时才构造**的函数内静态，
// 下面这个文件作用域对象的构造函数早于 main、也早于任何一次 `instance()`，
// 所以在单例落定前就把 HOME 换掉。（隔离 HOME 是本项目量「保留测试绿」时的既定口径。）
//
// 为什么不用 brief 点名的 `PkConfigStore::setConfigFilePathForTesting()`
// （pk/config/PkConfigStore.h:28）：它受 `PKCONFIG_ENABLE_TEST_HOOKS` 保护，而**主构建树
// 里这个宏没有打开** —— `pk/CMakeLists.txt`（复刻各 pk/*/CMakeLists.txt 库目标的那份）
// 漏了 `pk/config/CMakeLists.txt:66` 的
// `target_compile_definitions(pkconfig_objects PRIVATE PKCONFIG_ENABLE_TEST_HOOKS)`，
// 于是 `libpkconfig.a` 里根本没有那个符号（`nm -g` 实测 0 条），链接必失败。
// `pk/**` 不在本任务锁内 ⇒ 只登记不改，见 task-24-report.md。
//
// 只改配置落点，不改任何断言、不改判定逻辑。
namespace {
struct PkHomeRedirectForTesting {
    PkHomeRedirectForTesting()
    {
        std::error_code ec;
        const std::filesystem::path dir =
            std::filesystem::temp_directory_path() / "krita-pk-tiles3-tests"
                                                    / "kis_low_memory_tests";
        std::filesystem::create_directories(dir / "Library" / "Preferences", ec);
#ifdef _WIN32
        _putenv_s("USERPROFILE", dir.string().c_str());
        _putenv_s("APPDATA", (dir / "AppData" / "Roaming").string().c_str());
#else
        ::setenv("HOME", dir.c_str(), 1);
#endif
    }
};
[[maybe_unused]] PkHomeRedirectForTesting s_pkHomeRedirectForTesting;
}

#include "tiles_test_utils.h"
#include "tiles3/kis_tiled_data_manager.h"
#include "tiles3/kis_tile_data_store.h"
#include <kis_debug.h>
#include "config-limit-long-tests.h"

void KisLowMemoryTests::initTestCase()
{
    // hard limit of 1MiB, no undo in memory, no clones
    KisImageConfig config(false);
    config.setMemoryHardLimitPercent(1.1 * 100.0 / KisImageConfig::totalRAM());
    config.setMemorySoftLimitPercent(0);
    config.setMemoryPoolLimitPercent(0);
}


class DeadlockyThread : public QRunnable
{
public:
    enum Type {
        PRODUCER,
        CONSUMER_SRC,
        CONSUMER_DST
    };

    DeadlockyThread(Type type,
                    KisTiledDataManager &srcDM,
                    KisTiledDataManager &dstDM,
                    int numTiles,
                    int numCycles)
        : m_type(type),
          m_srcDM(srcDM),
          m_dstDM(dstDM),
          m_numTiles(numTiles),
          m_numCycles(numCycles)
    {
    }

    void run() override {
        switch(m_type) {
        case PRODUCER:
            for (int j = 0; j < m_numCycles; j++) {
                for (int i = 0; i < m_numTiles; i++) {
                    KisTileSP voidTile = m_srcDM.getTile(i, 0, true);
                    voidTile->lockForWrite();
                    QTest::qSleep(1);
                    voidTile->unlockForWrite();
                }

                PkRect cloneRect(0, 0, m_numTiles * 64, 64);
                m_dstDM.bitBltRough(&m_srcDM, cloneRect);

                if(j % 50 == 0) dbgKrita << "Producer:" << j << "of" << m_numCycles;

                KisTileDataStore::instance()->debugSwapAll();
            }
            break;
        case CONSUMER_SRC:
            for (int j = 0; j < m_numCycles; j++) {
                for (int i = 0; i < m_numTiles; i++) {
                    KisTileSP voidTile = m_srcDM.getTile(i, 0, false);
                    voidTile->lockForRead();
                    char temp = *voidTile->data();
                    Q_UNUSED(temp);
                    QTest::qSleep(1);
                    voidTile->unlockForRead();
                }

                if(j % 50 == 0) dbgKrita << "Consumer_src:" << j << "of" << m_numCycles;

                KisTileDataStore::instance()->debugSwapAll();
            }
            break;
        case CONSUMER_DST:
            for (int j = 0; j < m_numCycles; j++) {
                for (int i = 0; i < m_numTiles; i++) {
                    KisTileSP voidTile = m_dstDM.getTile(i, 0, false);
                    voidTile->lockForRead();
                    char temp = *voidTile->data();
                    Q_UNUSED(temp);
                    QTest::qSleep(1);
                    voidTile->unlockForRead();
                }

                if(j % 50 == 0) dbgKrita << "Consumer_dst:" << j << "of" << m_numCycles;

                KisTileDataStore::instance()->debugSwapAll();
            }

        }
    }

private:
    Type m_type;
    KisTiledDataManager &m_srcDM;
    KisTiledDataManager &m_dstDM;
    int m_numTiles;
    int m_numCycles;
};

void KisLowMemoryTests::readWriteOnSharedTiles()
{
    quint8 defaultPixel = 0;
    KisTiledDataManager srcDM(1, &defaultPixel);
    KisTiledDataManager dstDM(1, &defaultPixel);

    const int NUM_TILES = 10;

#ifdef LIMIT_LONG_TESTS
    const int NUM_CYCLES = 800;
#else
    const int NUM_CYCLES = 10000;
#endif

    QThreadPool pool;
    pool.setMaxThreadCount(10);

    pool.start(new DeadlockyThread(DeadlockyThread::PRODUCER,
                                   srcDM, dstDM, NUM_TILES, NUM_CYCLES));

    for (int i = 0; i < 4; i++) {
        pool.start(new DeadlockyThread(DeadlockyThread::CONSUMER_SRC,
                                       srcDM, dstDM, NUM_TILES, NUM_CYCLES));
        pool.start(new DeadlockyThread(DeadlockyThread::CONSUMER_DST,
                                       srcDM, dstDM, NUM_TILES, NUM_CYCLES));
    }

    pool.waitForDone();
}

void KisLowMemoryTests::hangingTilesTest()
{
    quint8 defaultPixel = 0;
    KisTiledDataManager srcDM(1, &defaultPixel);

    KisTileSP srcTile = srcDM.getTile(0, 0, true);

    srcTile->lockForWrite();
    srcTile->lockForRead();


    KisTiledDataManager dstDM(1, &defaultPixel);
    dstDM.bitBlt(&srcDM, PkRect(0,0,64,64));

    KisTileSP dstTile = dstDM.getTile(0, 0, true);

    dstTile->lockForRead();
    KisTileData *weirdTileData = dstTile->tileData();
    quint8 *weirdData = dstTile->data();

    QCOMPARE(weirdTileData, srcTile->tileData());
    QCOMPARE(weirdData, srcTile->data());

    KisTileDataStore::instance()->debugSwapAll();
    QCOMPARE(srcTile->tileData(), weirdTileData);
    QCOMPARE(dstTile->tileData(), weirdTileData);
    QCOMPARE(srcTile->data(), weirdData);
    QCOMPARE(dstTile->data(), weirdData);

    dstTile->lockForWrite();
    KisTileData *cowedTileData = dstTile->tileData();
    quint8 *cowedData = dstTile->data();

    QVERIFY(cowedTileData != weirdTileData);

    KisTileDataStore::instance()->debugSwapAll();
    QCOMPARE(srcTile->tileData(), weirdTileData);
    QCOMPARE(dstTile->tileData(), cowedTileData);
    QCOMPARE(srcTile->data(), weirdData);
    QCOMPARE(dstTile->data(), cowedData);

    QCOMPARE((int)weirdTileData->m_usersCount, 2);

    srcTile->unlockForWrite();
    srcTile->unlockForRead();
    srcTile = 0;

    srcDM.clear();

    KisTileDataStore::instance()->debugSwapAll();
    QCOMPARE(dstTile->tileData(), cowedTileData);
    QCOMPARE(dstTile->data(), cowedData);

    // two crash tests
    QCOMPARE(weirdTileData->data(), weirdData);
    quint8 testPixel = *weirdData;
    QCOMPARE(testPixel, defaultPixel);

    QCOMPARE((int)weirdTileData->m_usersCount, 1);

    dstTile->unlockForWrite();
    dstTile->unlockForRead();
    dstTile = 0;
}

SIMPLE_TEST_MAIN(KisLowMemoryTests)
