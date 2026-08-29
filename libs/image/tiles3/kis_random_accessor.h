/*
 *  SPDX-FileCopyrightText: 2006,2010 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_TILED_RANDOM_ACCESSOR_H
#define KIS_TILED_RANDOM_ACCESSOR_H

#include <cstdint>


#include <kis_shared.h>

#include "kis_tiled_data_manager.h"
#include "kis_random_accessor_ng.h"
#include "kis_iterator_complete_listener.h"


class KisRandomAccessor2 : public KisRandomAccessorNG
{

    struct KisTileInfo {
        KisTileSP tile;
        KisTileSP oldtile;
        std::uint8_t* data;
        const std::uint8_t* oldData;
        std::int32_t area_x1, area_y1, area_x2, area_y2;
    };

public:

    KisRandomAccessor2(KisTiledDataManager *ktm, std::int32_t offsetX, std::int32_t offsetY, bool writable, KisIteratorCompleteListener *completeListener);
    KisRandomAccessor2(const KisTiledRandomAccessor& lhs);
    ~KisRandomAccessor2() override;


private:
    inline void lockTile(KisTileSP &tile) {
        if (m_writable)
            tile->lockForWrite();
        else
            tile->lockForRead();
    }

    inline void lockOldTile(KisTileSP &tile) {
        // Doesn't depend on access type
        tile->lockForRead();
    }

    inline void unlockTile(KisTileSP &tile) {
        if (m_writable) {
            tile->unlockForWrite();
        } else {
            tile->unlockForRead();
        }
    }

    inline void unlockOldTile(KisTileSP &tile) {
        tile->unlockForRead();
    }

    inline std::uint32_t xToCol(std::uint32_t x) const {
        return m_ktm ? m_ktm->xToCol(x) : 0;
    }
    inline std::uint32_t yToRow(std::uint32_t y) const {
        return m_ktm ? m_ktm->yToRow(y) : 0;
    }

    KisTileInfo* fetchTileData(std::int32_t col, std::int32_t row);

public:
    /// Move to a given x,y position, fetch tiles and data
    void moveTo(std::int32_t x, std::int32_t y) override;
    std::uint8_t* rawData() override;
    const std::uint8_t* oldRawData() const override;
    const std::uint8_t* rawDataConst() const override;
    std::int32_t numContiguousColumns(std::int32_t x) const override;
    std::int32_t numContiguousRows(std::int32_t y) const override;
    std::int32_t rowStride(std::int32_t x, std::int32_t y) const override;
    std::int32_t x() const override;
    std::int32_t y() const override;

private:
    KisTiledDataManager *m_ktm;
    KisTileInfo** m_tilesCache;
    std::uint32_t m_tilesCacheSize;
    std::int32_t m_pixelSize;
    std::uint8_t* m_data;
    const std::uint8_t* m_oldData;
    bool m_writable;
    int m_lastX, m_lastY;
    std::int32_t m_offsetX, m_offsetY;
    KisIteratorCompleteListener *m_completeListener;
    static const std::uint32_t CACHESIZE; // Define the number of tiles we keep in cache

};

#endif
