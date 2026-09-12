/*
 *  SPDX-FileCopyrightText: 2010 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_TILED_DATA_MANAGER_TEST_H
#define KIS_TILED_DATA_MANAGER_TEST_H

#include <simpletest.h>

// 三个 check* 助手的矩形形参已迁 PkRect（内核 KisTiledDataManager 的
// clear/bitBlt/extent 等接口收 PkRect），所以本头要能看见 PkRect 的完整定义。
// 本头被 qt 桶 TU 消费（kis_tiled_data_manager_test.cpp 带 -DQT_CORE_LIB、
// 无 pk/*/compat），故补真 Pk 头（IMPACT §1 的桶纪律）。
#include <PkRect.h>

class KisTiledDataManager;

class KisTiledDataManagerTest : public QObject
{
    Q_OBJECT

private:
    bool checkHole(quint8* buffer, quint8 holeColor, PkRect holeRect,
                   quint8 backgroundColor, PkRect backgroundRect);

    bool checkTilesShared(KisTiledDataManager *srcDM,
                          KisTiledDataManager *dstDM,
                          bool takeOldSrc, bool takeOldDst,
                          PkRect tilesRect);

    bool checkTilesNotShared(KisTiledDataManager *srcDM,
                             KisTiledDataManager *dstDM,
                             bool takeOldSrc, bool takeOldDst,
                             PkRect tilesRect);

    void benchmarkCOWImpl();

private Q_SLOTS:
    void testUndoingNewTiles();
    void testPurgedAndEmptyTransactions();
    void testUnversionedBitBlt();
    void testVersionedBitBlt();
    void testBitBltOldData();
    void testBitBltRough();
    void testTransactions();
    void testPurgeHistory();
    void testUndoSetDefaultPixel();

    void benchmarkReadOnlyTileLazy();
    void benchmarkSharedPointers();

    void benchmarkCOWNoPooler();
    void benchmarkCOWWithPooler();

    void stressTest();

    void stressTestLazyCopying();

    void stressTestExtentsColumn();

    void benchmarkQRegion();
    void benchmarkKisRegion();
    void benchmarkOverlappedKisRegion();
};

#endif /* KIS_TILED_DATA_MANAGER_TEST_H */

