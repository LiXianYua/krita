/*
 *  SPDX-FileCopyrightText: 2010 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_swapped_data_store_test.h"
#include <simpletest.h>

#include <QRandomGenerator>

#include "kis_debug.h"

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
                                                    / "kis_swapped_data_store_test";
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


#include "tiles3/kis_tile_data.h"
#include "tiles_test_utils.h"

#include "tiles3/kis_tile_data_store.h"


#define COLUMN2COLOR(col) (col%255)

void KisSwappedDataStoreTest::testRoundTrip()
{
    const qint32 pixelSize = 1;
    const quint8 defaultPixel = 128;
    const qint32 NUM_TILES = 10000;

    KisImageConfig config(false);
    config.setMaxSwapSize(4);
    config.setSwapSlabSize(1);
    config.setSwapWindowSize(1);


    KisSwappedDataStore store;

    QList<KisTileData*> tileDataList;
    for(qint32 i = 0; i < NUM_TILES; i++)
        tileDataList.append(new KisTileData(pixelSize, &defaultPixel, KisTileDataStore::instance()));

    for(qint32 i = 0; i < NUM_TILES; i++) {
        KisTileData *td = tileDataList[i];
        QVERIFY(memoryIsFilled(defaultPixel, td->data(), TILESIZE));

        memset(td->data(), COLUMN2COLOR(i), TILESIZE);
        QVERIFY(memoryIsFilled(COLUMN2COLOR(i), td->data(), TILESIZE));

        // FIXME: take a lock of the tile data
        QVERIFY(store.trySwapOutTileData(td));
    }

    store.debugStatistics();

    for(qint32 i = 0; i < NUM_TILES; i++) {
        KisTileData *td = tileDataList[i];
        QVERIFY(!td->data());
        // TODO: check num clones

        // FIXME: take a lock of the tile data
        store.swapInTileData(td);
        QVERIFY(memoryIsFilled(COLUMN2COLOR(i), td->data(), TILESIZE));
    }

    store.debugStatistics();

    for(qint32 i = 0; i < NUM_TILES; i++)
        delete tileDataList[i];
}

void KisSwappedDataStoreTest::processTileData(qint32 column, KisTileData *td, KisSwappedDataStore &store)
{
    if(td->data()) {
        memset(td->data(), COLUMN2COLOR(column), TILESIZE);
        QVERIFY(memoryIsFilled(COLUMN2COLOR(column), td->data(), TILESIZE));

        // FIXME: take a lock of the tile data
        QVERIFY(store.trySwapOutTileData(td));
    }
    else {
        // TODO: check num clones
        // FIXME: take a lock of the tile data
        store.swapInTileData(td);
        QVERIFY(memoryIsFilled(COLUMN2COLOR(column), td->data(), TILESIZE));
    }
}

void KisSwappedDataStoreTest::testRandomAccess()
{
    QRandomGenerator rng(10);
    const qint32 pixelSize = 1;
    const quint8 defaultPixel = 128;
    const qint32 NUM_CYCLES = 50000;
    const qint32 NUM_TILES = 10000;

    KisImageConfig config(false);
    config.setMaxSwapSize(40);
    config.setSwapSlabSize(1);
    config.setSwapWindowSize(1);


    KisSwappedDataStore store;

    QList<KisTileData*> tileDataList;
    for(qint32 i = 0; i < NUM_TILES; i++)
        tileDataList.append(new KisTileData(pixelSize, &defaultPixel, KisTileDataStore::instance()));

    for(qint32 i = 0; i < NUM_CYCLES; i++) {
        if(!(i%5000))
            dbgKrita << i << "of" << NUM_CYCLES;

        qint32 col = rng.bounded(NUM_TILES);

        KisTileData *td = tileDataList[col];
        processTileData(col, td, store);
    }

    store.debugStatistics();

    for(qint32 i = 0; i < NUM_TILES; i++)
        delete tileDataList[i];
}

SIMPLE_TEST_MAIN(KisSwappedDataStoreTest)

