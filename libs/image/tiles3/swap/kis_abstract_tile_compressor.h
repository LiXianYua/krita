/*
 *  SPDX-FileCopyrightText: 2010 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_ABSTRACT_TILE_COMPRESSOR_H
#define __KIS_ABSTRACT_TILE_COMPRESSOR_H

#include <cstdint>

#include "kritaimage_export.h"
#include "../kis_tile.h"
#include "../kis_tiled_data_manager.h"

class KisPaintDeviceWriter;
class PkStream;
/**
 * Base class for compressing a tile and wrapping it with a header
 */

class KisAbstractTileCompressor;
typedef KisSharedPtr<KisAbstractTileCompressor> KisAbstractTileCompressorSP;

class KRITAIMAGE_EXPORT KisAbstractTileCompressor : public KisShared
{
public:
    KisAbstractTileCompressor();
    virtual ~KisAbstractTileCompressor();

public:

    /**
     * Compresses the \a tile and writes it into the \a stream.
     * Used by datamanager in load/save routines
     *
     * \see compressTile()
     */
    virtual bool writeTile(KisTileSP tile, KisPaintDeviceWriter &store) = 0;

    /**
     * Decompresses the \a tile from the \a stream.
     * Used by datamanager in load/save routines
     *
     * \see decompressTile()
     */
    virtual bool readTile(PkStream *stream, KisTiledDataManager *dm) = 0;

    /**
     * Compresses a \p tileData and writes it into the \p buffer.
     * The buffer must be at least tileDataBufferSize() bytes long.
     * Actual number of bytes written is returned using out-parameter
     * \p bytesWritten
     *
     * \param tileData an existing tile data. It should be created
     * and acquired by the caller.
     * \param buffer the buffer
     * \param bufferSize the size of the buffer
     * \param bytesWritten the number of written bytes
     *
     * \see tileDataBufferSize()
     */
    virtual void compressTileData(KisTileData *tileData,std::uint8_t *buffer,
                                  std::int32_t bufferSize, std::int32_t &bytesWritten) = 0;

    /**
     * Decompresses a \p tileData from a given \p buffer.
     *
     * \param buffer the buffer
     * \param bufferSize the size of the buffer
     * \param tileData an existing tile data where the result
     * will be written to. It should be created and acquired
     * by the caller.
     *
     */
    virtual bool decompressTileData(std::uint8_t *buffer, std::int32_t bufferSize,
                                    KisTileData *tileData) = 0;

    /**
     * Return the number of bytes needed for compressing one tile
     */
    virtual std::int32_t tileDataBufferSize(KisTileData *tileData) = 0;

protected:
    inline std::int32_t xToCol(KisTiledDataManager *dm, std::int32_t x) {
        return dm->xToCol(x);
    }

    inline std::int32_t yToRow(KisTiledDataManager *dm, std::int32_t y) {
        return dm->yToRow(y);
    }

    inline std::int32_t pixelSize(KisTiledDataManager *dm) {
        return dm->pixelSize();
    }
};

#endif /* __KIS_ABSTRACT_TILE_COMPRESSOR_H */
