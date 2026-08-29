/*
 *  SPDX-FileCopyrightText: 2010 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_TILE_COMPRESSOR_2_H
#define __KIS_TILE_COMPRESSOR_2_H

#include <cstdint>

#include "kis_abstract_tile_compressor.h"
#include <PkString.h>
#include <PkAuxTypes.h>

class KisAbstractCompression;

class KRITAIMAGE_EXPORT KisTileCompressor2 : public KisAbstractTileCompressor
{
public:
    KisTileCompressor2();
    ~KisTileCompressor2() override;

    bool writeTile(KisTileSP tile, KisPaintDeviceWriter &store) override;
    bool readTile(PkStream *io, KisTiledDataManager *dm) override;


    void compressTileData(KisTileData *tileData,std::uint8_t *buffer,
                          std::int32_t bufferSize, std::int32_t &bytesWritten) override;
    bool decompressTileData(std::uint8_t *buffer, std::int32_t bufferSize, KisTileData *tileData) override;
    std::int32_t tileDataBufferSize(KisTileData *tileData) override;

private:
    /**
     * Quite self describing
     */
    std::int32_t maxHeaderLength();

    PkString getHeader(KisTileSP tile, std::int32_t compressedSize);

    void prepareWorkBuffers(std::int32_t tileDataSize);
    void prepareStreamingBuffer(std::int32_t tileDataSize);

private:
    static const std::int8_t RAW_DATA_FLAG = 0;
    static const std::int8_t COMPRESSED_DATA_FLAG = 1;

private:
    PkByteArray m_linearizationBuffer;
    PkByteArray m_compressionBuffer;
    PkByteArray m_streamingBuffer;
    KisAbstractCompression *m_compression;
    static const PkString m_compressionName;
};

#endif /* __KIS_TILE_COMPRESSOR_2_H */
