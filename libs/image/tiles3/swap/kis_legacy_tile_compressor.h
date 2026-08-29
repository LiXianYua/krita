/*
 *  SPDX-FileCopyrightText: 2010 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_LEGACY_TILE_COMPRESSOR_H
#define __KIS_LEGACY_TILE_COMPRESSOR_H

#include <cstdint>

#include "kis_abstract_tile_compressor.h"


class KRITAIMAGE_EXPORT KisLegacyTileCompressor : public KisAbstractTileCompressor
{
public:
    KisLegacyTileCompressor();
    ~KisLegacyTileCompressor() override;

    bool writeTile(KisTileSP tile, KisPaintDeviceWriter &store) override;
    bool readTile(PkStream *stream, KisTiledDataManager *dm) override;


    void compressTileData(KisTileData *tileData,std::uint8_t *buffer,
                          std::int32_t bufferSize, std::int32_t &bytesWritten) override;
    bool decompressTileData(std::uint8_t *buffer, std::int32_t bufferSize, KisTileData *tileData) override;
    std::int32_t tileDataBufferSize(KisTileData *tileData) override;

private:
    /**
     * Quite self describing
     */
    std::int32_t maxHeaderLength();

    /**
     * Writes header into the buffer. Buffer size
     * should be maxHeaderLength() + 1 bytes at least
     * (to fit terminating '\0')
     */
    bool writeHeader(KisTileSP tile, std::uint8_t *buffer);
};

#endif /* __KIS_LEGACY_TILE_COMPRESSOR_H */
