/*
 *  SPDX-FileCopyrightText: 2010 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <cstdint>
#include <cassert>

#include "kis_legacy_tile_compressor.h"
#include "kis_paint_device_writer.h"
#include <PkStream.h>
#include <memory>

#define TILE_DATA_SIZE(pixelSize) ((pixelSize) * KisTileData::WIDTH * KisTileData::HEIGHT)

KisLegacyTileCompressor::KisLegacyTileCompressor()
{
}

KisLegacyTileCompressor::~KisLegacyTileCompressor()
{
}

bool KisLegacyTileCompressor::writeTile(KisTileSP tile, KisPaintDeviceWriter &store)
{
    const std::int32_t tileDataSize = TILE_DATA_SIZE(tile->pixelSize());

    const std::int32_t bufferSize = maxHeaderLength() + 1;
    std::unique_ptr<std::uint8_t[]> headerBuffer(new std::uint8_t[bufferSize]);

    bool retval = writeHeader(tile, headerBuffer.get());
    PK_TILES_ASSERT(retval);  // currently the code returns true unconditionally
    if (!retval) {
        return false;
    }

    store.write((char *)headerBuffer.get(), strlen((char *)headerBuffer.get()));

    tile->lockForRead();
    retval = store.write((char *)tile->data(), tileDataSize);
    tile->unlockForRead();

    return retval;
}

bool KisLegacyTileCompressor::readTile(PkStream *stream, KisTiledDataManager *dm)
{
    const std::int32_t tileDataSize = TILE_DATA_SIZE(pixelSize(dm));

    const std::int32_t bufferSize = maxHeaderLength() + 1;
    std::uint8_t *headerBuffer = new std::uint8_t[bufferSize];

    std::int32_t x, y;
    std::int32_t width, height;

    stream->readLine((char *)headerBuffer, bufferSize);
    sscanf((char *) headerBuffer, "%d,%d,%d,%d", &x, &y, &width, &height);

    std::int32_t row = yToRow(dm, y);
    std::int32_t col = xToCol(dm, x);

    KisTileSP tile = dm->getTile(col, row, true);

    tile->lockForWrite();
    stream->read((char *)tile->data(), tileDataSize);
    tile->unlockForWrite();

    return true;
}

void KisLegacyTileCompressor::compressTileData(KisTileData *tileData,
                                               std::uint8_t *buffer,
                                               std::int32_t bufferSize,
                                               std::int32_t &bytesWritten)
{
    bytesWritten = 0;
    const std::int32_t tileDataSize = TILE_DATA_SIZE(tileData->pixelSize());
    (void)(bufferSize);
    PK_TILES_ASSERT(bufferSize >= tileDataSize);
    memcpy(buffer, tileData->data(), tileDataSize);
    bytesWritten += tileDataSize;
}

bool KisLegacyTileCompressor::decompressTileData(std::uint8_t *buffer,
                                                 std::int32_t bufferSize,
                                                 KisTileData *tileData)
{
    const std::int32_t tileDataSize = TILE_DATA_SIZE(tileData->pixelSize());
    if (bufferSize >= tileDataSize) {
        memcpy(tileData->data(), buffer, tileDataSize);
        return true;
    }
    return false;
}

std::int32_t KisLegacyTileCompressor::tileDataBufferSize(KisTileData *tileData)
{
    return TILE_DATA_SIZE(tileData->pixelSize());
}

inline std::int32_t KisLegacyTileCompressor::maxHeaderLength()
{
    static const std::int32_t LEGACY_MAGIC_NUMBER = 79;
    return LEGACY_MAGIC_NUMBER;
}

inline bool KisLegacyTileCompressor::writeHeader(KisTileSP tile,
                                                 std::uint8_t *buffer)
{
    std::int32_t x, y;
    std::int32_t width, height;

    tile->extent().getRect(&x, &y, &width, &height);
    snprintf((char *)buffer, (maxHeaderLength() + 1), "%d,%d,%d,%d\n", x, y, width, height);

    return true;
}
