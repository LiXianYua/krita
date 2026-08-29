/*
 *  copyright (c) 2006,2010 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <cstdint>
#include <cassert>

#include "kis_random_accessor.h"


#include <kis_debug.h>


const std::uint32_t KisRandomAccessor2::CACHESIZE = 4; // Define the number of tiles we keep in cache

KisRandomAccessor2::KisRandomAccessor2(KisTiledDataManager *ktm, std::int32_t offsetX, std::int32_t offsetY, bool writable, KisIteratorCompleteListener *completeListener) :
        m_ktm(ktm),
        m_tilesCache(new KisTileInfo*[CACHESIZE]),
        m_tilesCacheSize(0),
        m_pixelSize(m_ktm->pixelSize()),
        m_data(0),
        m_oldData(0),
        m_writable(writable),
        m_lastX(0),
        m_lastY(0),
        m_offsetX(offsetX),
        m_offsetY(offsetY),
        m_completeListener(completeListener)
{
    assert(ktm != 0);
}

KisRandomAccessor2::~KisRandomAccessor2()
{
    for (uint i = 0; i < m_tilesCacheSize; i++) {
        unlockTile(m_tilesCache[i]->tile);
        unlockOldTile(m_tilesCache[i]->oldtile);
        delete m_tilesCache[i];
    }
    delete [] m_tilesCache;

    if (m_writable && m_completeListener) {
        m_completeListener->notifyWritableIteratorCompleted();
    }
}

void KisRandomAccessor2::moveTo(std::int32_t x, std::int32_t y)
{
    m_lastX = x;
    m_lastY = y;

    x -= m_offsetX;
    y -= m_offsetY;

    // Look in the cache if the tile if the data is available
    for (uint i = 0; i < m_tilesCacheSize; i++) {
        if (x >= m_tilesCache[i]->area_x1 && x <= m_tilesCache[i]->area_x2 &&
                y >= m_tilesCache[i]->area_y1 && y <= m_tilesCache[i]->area_y2) {
            KisTileInfo* kti = m_tilesCache[i];
            std::uint32_t offset = x - kti->area_x1 + (y - kti->area_y1) * KisTileData::WIDTH;
            offset *= m_pixelSize;
            m_data = kti->data + offset;
            m_oldData = kti->oldData + offset;
            if (i > 0) {
                memmove(m_tilesCache + 1, m_tilesCache, i * sizeof(KisTileInfo*));
                m_tilesCache[0] = kti;
            }
            return;
        }
    }
    // The tile wasn't in cache
    if (m_tilesCacheSize == KisRandomAccessor2::CACHESIZE) { // Remove last element of cache
        unlockTile(m_tilesCache[CACHESIZE-1]->tile);
        unlockOldTile(m_tilesCache[CACHESIZE-1]->oldtile);
        delete m_tilesCache[CACHESIZE-1];
    } else {
        m_tilesCacheSize++;
    }
    std::uint32_t col = xToCol(x);
    std::uint32_t row = yToRow(y);
    KisTileInfo* kti = fetchTileData(col, row);
    std::uint32_t offset = x - kti->area_x1 + (y - kti->area_y1) * KisTileData::WIDTH;
    offset *= m_pixelSize;
    m_data = kti->data + offset;
    m_oldData = kti->oldData + offset;
    memmove(m_tilesCache + 1, m_tilesCache, (KisRandomAccessor2::CACHESIZE - 1) * sizeof(KisTileInfo*));
    m_tilesCache[0] = kti;
}


std::uint8_t* KisRandomAccessor2::rawData()
{
    return m_data;
}


const std::uint8_t* KisRandomAccessor2::oldRawData() const
{
#ifdef DEBUG
    if (!m_ktm->hasCurrentMemento()) warnTiles << "Accessing oldRawData() when no transaction is in progress.";
#endif
    return m_oldData;
}

const std::uint8_t* KisRandomAccessor2::rawDataConst() const
{
    return m_data;
}

KisRandomAccessor2::KisTileInfo* KisRandomAccessor2::fetchTileData(std::int32_t col, std::int32_t row)
{
    KisTileInfo* kti = new KisTileInfo;

    m_ktm->getTilesPair(col, row, m_writable, &kti->tile, &kti->oldtile);

    lockTile(kti->tile);
    kti->data = kti->tile->data();

    lockOldTile(kti->oldtile);
    kti->oldData = kti->oldtile->data();

    kti->area_x1 = col * KisTileData::HEIGHT;
    kti->area_y1 = row * KisTileData::WIDTH;
    kti->area_x2 = kti->area_x1 + KisTileData::HEIGHT - 1;
    kti->area_y2 = kti->area_y1 + KisTileData::WIDTH - 1;

    return kti;
}

std::int32_t KisRandomAccessor2::numContiguousColumns(std::int32_t x) const
{
    return m_ktm->numContiguousColumns(x - m_offsetX, 0, 0);
}

std::int32_t KisRandomAccessor2::numContiguousRows(std::int32_t y) const
{
    return m_ktm->numContiguousRows(y - m_offsetY, 0, 0);
}

std::int32_t KisRandomAccessor2::rowStride(std::int32_t x, std::int32_t y) const
{
    return m_ktm->rowStride(x - m_offsetX, y - m_offsetY);
}

std::int32_t KisRandomAccessor2::x() const
{
    return m_lastX;
}

std::int32_t KisRandomAccessor2::y() const
{
    return m_lastY;
}
