/*
 *  SPDX-FileCopyrightText: 2010 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <cstdint>
#include <cassert>

#include "kis_tile_compressor_2.h"
#include "kis_lzf_compression.h"
#include <PkStream.h>
#include "kis_paint_device_writer.h"
#include <cstdio>
#define TILE_DATA_SIZE(pixelSize) ((pixelSize) * KisTileData::WIDTH * KisTileData::HEIGHT)

const PkString KisTileCompressor2::m_compressionName = "LZF";


KisTileCompressor2::KisTileCompressor2()
{
    m_compression = new KisLzfCompression();
}

KisTileCompressor2::~KisTileCompressor2()
{
    delete m_compression;
}

bool KisTileCompressor2::writeTile(KisTileSP tile, KisPaintDeviceWriter &store)
{
    const std::int32_t tileDataSize = TILE_DATA_SIZE(tile->pixelSize());
    prepareStreamingBuffer(tileDataSize);

    std::int32_t bytesWritten;

    tile->lockForRead();
    compressTileData(tile->tileData(), (std::uint8_t*)m_streamingBuffer.data(),
                     m_streamingBuffer.size(), bytesWritten);
    tile->unlockForRead();

    PkString header = getHeader(tile, bytesWritten);
    bool retval = true;
    const std::string headerUtf8 = header.PkToUtf8();
    retval = store.write(headerUtf8.data(), static_cast<long long>(headerUtf8.size()));
    if (!retval) {
        warnFile << "Failed to write the tile header";
    }
    retval = store.write(m_streamingBuffer.data(), bytesWritten);
    if (!retval) {
        warnFile << "Failed to write the tile data";
    }
    return retval;
}

bool KisTileCompressor2::readTile(PkStream *stream, KisTiledDataManager *dm)
{
    const std::int32_t tileDataSize = TILE_DATA_SIZE(pixelSize(dm));
    prepareStreamingBuffer(tileDataSize);

    // The header line is "x,y,compressionName,dataSize\n", at most
    // maxHeaderLength() bytes.  readLine() stops at '\n' (or the buffer
    // limit), so a fixed-size buffer cannot swallow the tile data that
    // follows the header on the same stream.
    char headerBuf[64] = {0};
    if (stream->readLine(headerBuf, sizeof(headerBuf)) <= 0) {
        return false;
    }

    std::int32_t x = 0, y = 0, dataSize = 0;
    char compressionName[16] = {0};
    if (sscanf(headerBuf, "%d,%d,%15s,%d", &x, &y, compressionName, &dataSize) != 4) {
        return false;
    }

    if (PkString(compressionName) != m_compressionName) {
        return false;
    }

    std::int32_t row = yToRow(dm, y);
    std::int32_t col = xToCol(dm, x);

    KisTileSP tile = dm->getTile(col, row, true);

    stream->read(m_streamingBuffer.data(), dataSize);

    tile->lockForWrite();
    bool res = decompressTileData((std::uint8_t*)m_streamingBuffer.data(), dataSize, tile->tileData());
    tile->unlockForWrite();
    return res;
}

void KisTileCompressor2::prepareStreamingBuffer(std::int32_t tileDataSize)
{
    /**
     * TODO: delete this buffer!
     * It is better to use one of other two buffers to store streams
     */
    m_streamingBuffer.resize(tileDataSize + 1);
}

void KisTileCompressor2::prepareWorkBuffers(std::int32_t tileDataSize)
{
    const std::int32_t bufferSize = m_compression->outputBufferSize(tileDataSize);

    if (m_linearizationBuffer.size() < tileDataSize) {
        m_linearizationBuffer.resize(tileDataSize);
    }

    if (m_compressionBuffer.size() < bufferSize) {
        m_compressionBuffer.resize(bufferSize);
    }
}

void KisTileCompressor2::compressTileData(KisTileData *tileData,
                                          std::uint8_t *buffer,
                                          std::int32_t bufferSize,
                                          std::int32_t &bytesWritten)
{
    const std::int32_t pixelSize = tileData->pixelSize();
    const std::int32_t tileDataSize = TILE_DATA_SIZE(pixelSize);
    std::int32_t compressedBytes;

    (void)(bufferSize);
    PK_TILES_ASSERT(bufferSize >= tileDataSize + 1);

    prepareWorkBuffers(tileDataSize);

    KisAbstractCompression::linearizeColors(tileData->data(), (std::uint8_t*)m_linearizationBuffer.data(),
                                            tileDataSize, pixelSize);

    compressedBytes = m_compression->compress((std::uint8_t*)m_linearizationBuffer.data(), tileDataSize,
                                              (std::uint8_t*)m_compressionBuffer.data(), m_compressionBuffer.size());

    if(compressedBytes < tileDataSize) {
        buffer[0] = COMPRESSED_DATA_FLAG;
        memcpy(buffer + 1, m_compressionBuffer.data(), compressedBytes);
        bytesWritten = compressedBytes + 1;
    }
    else {
        buffer[0] = RAW_DATA_FLAG;
        memcpy(buffer + 1, tileData->data(), tileDataSize);
        bytesWritten = tileDataSize + 1;
    }
}

bool KisTileCompressor2::decompressTileData(std::uint8_t *buffer,
                                            std::int32_t bufferSize,
                                            KisTileData *tileData)
{
    const std::int32_t pixelSize = tileData->pixelSize();
    const std::int32_t tileDataSize = TILE_DATA_SIZE(pixelSize);

    if(buffer[0] == COMPRESSED_DATA_FLAG) {
        prepareWorkBuffers(tileDataSize);

        std::int32_t bytesWritten;
        bytesWritten = m_compression->decompress(buffer + 1, bufferSize - 1,
                                                 (std::uint8_t*)m_linearizationBuffer.data(), tileDataSize);
        if (bytesWritten == tileDataSize) {
            KisAbstractCompression::delinearizeColors((std::uint8_t*)m_linearizationBuffer.data(),
                                                      tileData->data(),
                                                      tileDataSize, pixelSize);
            return true;
        }
        return false;
    }
    else {
        memcpy(tileData->data(), buffer + 1, tileDataSize);
        return true;
    }
    return false;

}

std::int32_t KisTileCompressor2::tileDataBufferSize(KisTileData *tileData)
{
    return TILE_DATA_SIZE(tileData->pixelSize()) + 1;
}

inline std::int32_t KisTileCompressor2::maxHeaderLength()
{
    static const std::int32_t QINT32_LENGTH = 11;
    static const std::int32_t COMPRESSION_NAME_LENGTH = 5;
    static const std::int32_t SEPARATORS_LENGTH = 4;

    return 3 * QINT32_LENGTH + COMPRESSION_NAME_LENGTH + SEPARATORS_LENGTH;
}

inline PkString KisTileCompressor2::getHeader(KisTileSP tile,
                                              std::int32_t compressedSize)
{
    std::int32_t x, y;
    std::int32_t width, height;
    tile->extent().getRect(&x, &y, &width, &height);

    return PkString("%1,%2,%3,%4\n").arg(x).arg(y).arg(m_compressionName).arg(compressedSize);
}
