/*
 *  SPDX-FileCopyrightText: 2018 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisFrameSerializerTest.h"

#include <QtTest>

#include <KisFrameDataSerializer.h>
#include <PkRect.h>
#include <PkString.h>
#include "opengl/kis_texture_tile_info_pool.h"

#include <QDataStream>
#include <QDirIterator>
#include <QFile>
#include <QTemporaryDir>

#include <cstring>
#include <algorithm>
#include <cstdint>

#include <testutil.h>

#include <simpletest.h>

static const int maxTileSize = 256;

KisFrameDataSerializer::Frame generateTestFrame(int frameSeed, KisTextureTileInfoPoolSP pool)
{
    KisFrameDataSerializer::Frame frame;
    frame.pixelSize = 4;

    for (int i = 0; i < std::clamp(frameSeed * 5, 1, 100); i++) {
        KisFrameDataSerializer::FrameTile tile(pool);
        tile.col = i * 10;
        tile.row = i * 20;
        tile.rect = PkRect(i, 2 * i,
                           std::min(i * 5, maxTileSize),
                           std::min(i * 7, maxTileSize));
        tile.data.allocate(frame.pixelSize);

        const int numPixels = tile.rect.width() * tile.rect.height();
        std::int32_t *dataPtr = reinterpret_cast<std::int32_t*>(tile.data.data());

        for (int j = 0; j < numPixels; j++) {
            *dataPtr++ = frameSeed + j;
        }

        frame.frameTiles.push_back(std::move(tile));
    }

    return frame;
}

bool verifyTestFrame(int frameSeed, const KisFrameDataSerializer::Frame &frame)
{
    KIS_COMPARE_RF(frame.pixelSize, 4);
    KIS_COMPARE_RF(int(frame.frameTiles.size()), std::clamp(frameSeed * 5, 1, 100));

    for (int i = 0; i < int(frame.frameTiles.size()); i++) {
        const KisFrameDataSerializer::FrameTile &tile = frame.frameTiles[i];

        KIS_COMPARE_RF(tile.col, i * 10);
        KIS_COMPARE_RF(tile.row, i * 20);
        KIS_COMPARE_RF(tile.rect.x(), i);
        KIS_COMPARE_RF(tile.rect.y(), 2 * i);
        KIS_COMPARE_RF(tile.rect.width(), std::min(i * 5, maxTileSize));
        KIS_COMPARE_RF(tile.rect.height(), std::min(i * 7, maxTileSize));

        const int numPixels = tile.rect.width() * tile.rect.height();
        std::int32_t *dataPtr = reinterpret_cast<std::int32_t*>(tile.data.data());

        for (int j = 0; j < numPixels; j++) {
            KIS_COMPARE_RF(*dataPtr++, frameSeed + j);
        }
    }

    return true;
}



void KisFrameSerializerTest::testFrameDataSerialization()
{
    KisTextureTileInfoPoolRegistry poolRegistry;
    KisTextureTileInfoPoolSP pool = poolRegistry.getPool(maxTileSize, maxTileSize);


    KisFrameDataSerializer serializer;

    KisFrameDataSerializer::Frame testFrame1 = generateTestFrame(2, pool);
    KisFrameDataSerializer::Frame testFrame2 = generateTestFrame(3, pool);
    KisFrameDataSerializer::Frame testFrame3 = generateTestFrame(503, pool);
    int testFrameId1 = -1;
    int testFrameId2 = -1;
    int testFrameId3 = -1;



    testFrameId1 = serializer.saveFrame(testFrame1);
    QCOMPARE(serializer.hasFrame(testFrameId1), true);
    QCOMPARE(serializer.hasFrame(testFrameId2), false);
    QCOMPARE(serializer.hasFrame(testFrameId3), false);

    testFrameId2 = serializer.saveFrame(testFrame2);
    QCOMPARE(serializer.hasFrame(testFrameId1), true);
    QCOMPARE(serializer.hasFrame(testFrameId2), true);
    QCOMPARE(serializer.hasFrame(testFrameId3), false);

    testFrameId3 = serializer.saveFrame(testFrame3);
    QCOMPARE(serializer.hasFrame(testFrameId1), true);
    QCOMPARE(serializer.hasFrame(testFrameId2), true);
    QCOMPARE(serializer.hasFrame(testFrameId3), true);

    QVERIFY(verifyTestFrame(2, serializer.loadFrame(testFrameId1, pool)));
    QVERIFY(verifyTestFrame(3, serializer.loadFrame(testFrameId2, pool)));
    QVERIFY(verifyTestFrame(503, serializer.loadFrame(testFrameId3, pool)));

    serializer.forgetFrame(testFrameId2);
    QCOMPARE(serializer.hasFrame(testFrameId1), true);
    QCOMPARE(serializer.hasFrame(testFrameId2), false);
    QCOMPARE(serializer.hasFrame(testFrameId3), true);

    serializer.forgetFrame(testFrameId3);
    QCOMPARE(serializer.hasFrame(testFrameId1), true);
    QCOMPARE(serializer.hasFrame(testFrameId2), false);
    QCOMPARE(serializer.hasFrame(testFrameId3), false);

    serializer.forgetFrame(testFrameId1);
    QCOMPARE(serializer.hasFrame(testFrameId1), false);
    QCOMPARE(serializer.hasFrame(testFrameId2), false);
    QCOMPARE(serializer.hasFrame(testFrameId3), false);
}

void KisFrameSerializerTest::testFrameDataWireCompatibility()
{
    QTemporaryDir testRoot;
    QVERIFY(testRoot.isValid());

    KisTextureTileInfoPoolRegistry poolRegistry;
    KisTextureTileInfoPoolSP pool = poolRegistry.getPool(maxTileSize, maxTileSize);

    KisFrameDataSerializer::Frame frame;
    frame.pixelSize = 4;

    KisFrameDataSerializer::FrameTile tile(pool);
    tile.col = 17;
    tile.row = -9;
    tile.rect = PkRect(7, -3, 1, 1);
    tile.data.allocate(frame.pixelSize);
    const QByteArray expectedPayload = QByteArray::fromHex("12345678");
    std::memcpy(tile.data.data(), expectedPayload.constData(), expectedPayload.size());
    frame.frameTiles.push_back(std::move(tile));

    const QByteArray testRootUtf8 = testRoot.path().toUtf8();
    KisFrameDataSerializer serializer(PkString::PkFromUtf8(testRootUtf8.constData(), testRootUtf8.size()));
    const int expectedFrameId = serializer.saveFrame(frame);

    QStringList frameFiles;
    QDirIterator it(testRoot.path(), QStringList{QStringLiteral("frame_*")},
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        frameFiles.push_back(it.next());
    }
    QCOMPARE(frameFiles.size(), 1);

    QFile file(frameFiles.front());
    QVERIFY(file.open(QFile::ReadOnly));
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::BigEndian);
    stream.setVersion(QDataStream::Qt_5_15);

    qint32 frameId = -1;
    qint32 pixelSize = -1;
    qint32 tileCount = -1;
    qint32 col = 0;
    qint32 row = 0;
    qint32 left = 0;
    qint32 top = 0;
    qint32 right = 0;
    qint32 bottom = 0;
    bool compressed = true;
    qint32 payloadLength = -1;

    stream >> frameId >> pixelSize >> tileCount >> col >> row;
    stream >> left >> top >> right >> bottom;
    stream >> compressed >> payloadLength;

    QCOMPARE(frameId, expectedFrameId);
    QCOMPARE(pixelSize, 4);
    QCOMPARE(tileCount, 1);
    QCOMPARE(col, 17);
    QCOMPARE(row, -9);
    QCOMPARE(left, 7);
    QCOMPARE(top, -3);
    QCOMPARE(right, 7);
    QCOMPARE(bottom, -3);
    QCOMPARE(compressed, false);
    QCOMPARE(payloadLength, expectedPayload.size());

    QByteArray payload(payloadLength, Qt::Uninitialized);
    QCOMPARE(stream.readRawData(payload.data(), payload.size()), payload.size());
    QCOMPARE(payload, expectedPayload);
    QCOMPARE(stream.status(), QDataStream::Ok);
    QVERIFY(file.atEnd());
}

#include "kis_random_source.h"

void randomizeFrame(KisFrameDataSerializer::Frame &frame, double portion)
{
    // randomly reset 50% of the pixels
    KisRandomSource rnd(1);
    for (KisFrameDataSerializer::FrameTile &tile : frame.frameTiles) {
        const int numPixels = tile.rect.width() * tile.rect.height();
        std::int32_t *pixelPtr = reinterpret_cast<std::int32_t*>(tile.data.data());

        for (int j = 0; j < numPixels; j++) {
            if (rnd.generateNormalized() < portion) {
                (*pixelPtr) = 0;
            }

            pixelPtr++;
        }
    }
}

void KisFrameSerializerTest::testFrameUniquenessEstimation()
{
    KisTextureTileInfoPoolRegistry poolRegistry;
    KisTextureTileInfoPoolSP pool = poolRegistry.getPool(maxTileSize, maxTileSize);

    KisFrameDataSerializer::Frame testFrame1 = generateTestFrame(2, pool);
    KisFrameDataSerializer::Frame testFrame2 = generateTestFrame(2, pool);

    boost::optional<double> result;

    result = KisFrameDataSerializer::estimateFrameUniqueness(testFrame1, testFrame2, 0.1);
    QVERIFY(!!result);
    QVERIFY(qFuzzyCompare(*result, 0.0));

    KisFrameDataSerializer::Frame testFrame3 = generateTestFrame(3, pool);

    result = KisFrameDataSerializer::estimateFrameUniqueness(testFrame1, testFrame3, 0.1);
    QVERIFY(!result);

    // randomly reset 50% of the pixels
    randomizeFrame(testFrame2, 0.5);

    result = KisFrameDataSerializer::estimateFrameUniqueness(testFrame1, testFrame2, 0.01);
    QVERIFY(!!result);
    QVERIFY(*result >= 0.45);
    QVERIFY(*result <= 0.55);
}

void KisFrameSerializerTest::testFrameArithmetics()
{
    KisTextureTileInfoPoolRegistry poolRegistry;
    KisTextureTileInfoPoolSP pool = poolRegistry.getPool(maxTileSize, maxTileSize);

    KisFrameDataSerializer::Frame testFrame1 = generateTestFrame(2, pool);
    KisFrameDataSerializer::Frame testFrame2 = generateTestFrame(2, pool);
    randomizeFrame(testFrame2, 0.2);

    boost::optional<double> result =
        KisFrameDataSerializer::estimateFrameUniqueness(testFrame1, testFrame2, 0.01);

    QVERIFY(!!result);
    QVERIFY(*result >= 0.15);
    QVERIFY(*result <= 0.25);


    {
        KisFrameDataSerializer::Frame testFrame3 = generateTestFrame(2, pool);
        randomizeFrame(testFrame3, 0.2);

        const bool framesAreSame = KisFrameDataSerializer::subtractFrames(testFrame3, testFrame2);
        QVERIFY(framesAreSame);
    }

    {
        KisFrameDataSerializer::Frame testFrame3 = generateTestFrame(2, pool);
        randomizeFrame(testFrame3, 0.2);

        const bool framesAreSame = KisFrameDataSerializer::subtractFrames(testFrame3, testFrame1);
        QVERIFY(!framesAreSame);

        KisFrameDataSerializer::addFrames(testFrame3, testFrame1);

        result = KisFrameDataSerializer::estimateFrameUniqueness(testFrame3, testFrame2, 1.0);
        QVERIFY(!!result);
        QVERIFY(*result == 0.0);
    }
}

SIMPLE_TEST_MAIN(KisFrameSerializerTest)
