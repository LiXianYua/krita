/*
 *  SPDX-FileCopyrightText: 2009 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_TILE_DATA_WRAPPER_H
#define __KIS_TILE_DATA_WRAPPER_H

#include <cstdint>


/**
 * KisTileDataWrapper is a special object, that fetches the tile from
 * the data manager according to the position, locks it and returns
 * a pointer to the needed piece of data
 */
class KisTileDataWrapper
{
public:
    enum accessType {
        READ,
        WRITE
    };

    /**
     * Fetches the tile which contains point (\p x, \p y) from
     * the data manager \p dm with access \p type
     */
    inline KisTileDataWrapper(KisTiledDataManager *dm,
                              std::int32_t x, std::int32_t y,
                              enum KisTileDataWrapper::accessType type)
    {
        const std::int32_t col = dm->xToCol(x);
        const std::int32_t row = dm->yToRow(y);

        /* FIXME: Always positive? */
        const std::int32_t xInTile = x - col * KisTileData::WIDTH;
        const std::int32_t yInTile = y - row * KisTileData::HEIGHT;

        const std::int32_t pixelIndex = xInTile + yInTile * KisTileData::WIDTH;

        KisTileSP tile = dm->getTile(col, row, type == WRITE);

        m_tile = tile;
        m_offset = pixelIndex * dm->pixelSize();

        if (type == READ) {
            m_tile->lockForRead();
        }
        else {
            m_tile->lockForWrite();
        }

        m_type = type;
    }

    virtual ~KisTileDataWrapper()
    {
        if (m_type == READ) {
            m_tile->unlockForRead();
        } else {
            m_tile->unlockForWrite();
        }
    }

    /**
     * Returns the offset of the data in the tile's chunk of memory
     *
     * \see data()
     */
    inline std::int32_t offset() const
    {
        return m_offset;
    }

    /**
     * Returns the fetched tile
     */
    inline KisTileSP& tile()
    {
        return m_tile;
    }

    /**
     * Returns the pointer to the pixel, that was passed to
     * the constructor. This points to the raw data of the tile,
     * so you should think about the borders of the tile yourself.
     * When (x,y) is the top-left corner of the tile, the pointer
     * will lead to the beginning of the tile's chunk of memory.
     */
    inline std::uint8_t* data() const
    {
        return m_tile->data() + m_offset;
    }

private:
    KisTileDataWrapper(const KisTileDataWrapper &) = delete;
    KisTileDataWrapper &operator=(const KisTileDataWrapper &) = delete;

    KisTileSP m_tile;
    std::int32_t m_offset;
    KisTileDataWrapper::accessType m_type;
};
#endif /* __KIS_TILE_DATA_WRAPPER_H */
