/*
 *  SPDX-FileCopyrightText: 2010 Lukáš Tvrdý <lukast.dev@gmail.com>
 *  SPDX-FileCopyrightText: 2010 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <cstdint>
#include <cassert>

#include "kis_vline_iterator.h"

#include <iostream>

KisVLineIterator2::KisVLineIterator2(KisDataManager *dataManager, std::int32_t x, std::int32_t y, std::int32_t h, std::int32_t offsetX, std::int32_t offsetY, bool writable, KisIteratorCompleteListener *completeListener)
    : KisBaseIterator(dataManager, writable, completeListener),
      m_offsetX(offsetX),
      m_offsetY(offsetY)
{
    x -= m_offsetX;
    y -= m_offsetY;
    assert(dataManager != 0);

    assert(h > 0); // for us, to warn us when abusing the iterators
    if (h < 1) h = 1;  // for release mode, to make sure there's always at least one pixel read.

    m_lineStride = m_pixelSize * KisTileData::WIDTH;

    m_x = x;
    m_y = y;

    m_top = y;
    m_bottom = y + h - 1;

    m_left = m_x;

    m_havePixels = (h == 0) ? false : true;
    if (m_top > m_bottom) {
        m_havePixels = false;
        return;
    }

    m_topRow = yToRow(m_top);
    m_bottomRow = yToRow(m_bottom);

    m_column = xToCol(m_x);
    m_xInTile = calcXInTile(m_x, m_column);

    m_topInTopmostTile = m_top - m_topRow * KisTileData::WIDTH;

    m_tilesCacheSize = m_bottomRow - m_topRow + 1;
    m_tilesCache.resize(m_tilesCacheSize);

    m_tileSize = m_lineStride * KisTileData::HEIGHT;

    // let's preallocate first row
    for (int i = 0; i < m_tilesCacheSize; i++){
        fetchTileDataForCache(m_tilesCache[i], m_column, m_topRow + i);
    }
    m_index = 0;
    switchToTile(m_topInTopmostTile);
}

void KisVLineIterator2::resetPixelPos()
{
    m_y = m_top;

    m_index = 0;
    switchToTile(m_topInTopmostTile);

    m_havePixels = true;
}

void KisVLineIterator2::resetColumnPos()
{
    m_x = m_left;

    m_column = xToCol(m_x);
    m_xInTile = calcXInTile(m_x, m_column);
    preallocateTiles();

    resetPixelPos();
}

bool KisVLineIterator2::nextPixel()
{
    // We won't increment m_x here as integer can overflow here
    if (m_y >= m_bottom) {
        //return !m_isDoneFlag;
        return m_havePixels = false;
    } else {
        ++m_y;
        m_data += m_lineStride;
        if (m_data < m_dataBottom)
            m_oldData += m_lineStride;
        else {
            // Switching to the beginning of the next tile
            ++m_index;
            switchToTile(0);
        }
    }

    return m_havePixels;
}


void KisVLineIterator2::nextColumn()
{
    m_y = m_top;
    ++m_x;

    if (++m_xInTile < KisTileData::HEIGHT) {
        /* do nothing, usual case */
    } else {
        ++m_column;
        m_xInTile = 0;
        preallocateTiles();
    }
    m_index = 0;
    switchToTile(m_topInTopmostTile);

    m_havePixels = true;
}


std::int32_t KisVLineIterator2::nConseqPixels() const
{
    return 1;
}

bool KisVLineIterator2::nextPixels(std::int32_t n)
{
    assert((!(m_y > 0 && (m_y + n) < 0)) && "vlineIt+=: Integer overflow");

    std::int32_t previousRow = yToRow(m_y);
    // We won't increment m_x here first as integer can overflow
    if (m_y >= m_bottom || (m_y += n) > m_bottom) {
        m_havePixels = false;
    } else {
        std::int32_t row = yToRow(m_y);
        // if we are in the same column in tiles
        if (row == previousRow) {
            m_data += n * m_pixelSize;
        } else {
            std::int32_t yInTile = calcYInTile(m_y, row);
            m_index += row - previousRow;
            switchToTile(yInTile);
        }
    }
    return m_havePixels;
}



KisVLineIterator2::~KisVLineIterator2()
{
    for (int i = 0; i < m_tilesCacheSize; i++) {
        unlockTile(m_tilesCache[i].tile);
        unlockOldTile(m_tilesCache[i].oldtile);
    }
}


std::uint8_t* KisVLineIterator2::rawData()
{
    return m_data;
}


const std::uint8_t* KisVLineIterator2::oldRawData() const
{
    return m_oldData;
}

const std::uint8_t* KisVLineIterator2::rawDataConst() const
{
    return m_data;
}

void KisVLineIterator2::switchToTile(std::int32_t yInTile)
{
    // The caller must ensure that we are not out of bounds
    assert(m_index < m_tilesCacheSize);
    assert(m_index >= 0);

    int offset_row = m_pixelSize * m_xInTile;
    m_data = m_tilesCache[m_index].data;
    m_oldData = m_tilesCache[m_index].oldData;
    m_data += offset_row;
    m_dataBottom = m_data + m_tileSize;
    int offset_col = m_pixelSize * yInTile * KisTileData::WIDTH;
    m_data  += offset_col;
    m_oldData += offset_row + offset_col;
}


void KisVLineIterator2::fetchTileDataForCache(KisTileInfo& kti, std::int32_t col, std::int32_t row)
{
    m_dataManager->getTilesPair(col, row, m_writable, &kti.tile, &kti.oldtile);

    lockTile(kti.tile);
    kti.data = kti.tile->data();

    lockOldTile(kti.oldtile);
    kti.oldData = kti.oldtile->data();
}

void KisVLineIterator2::preallocateTiles()
{
    for (int i = 0; i < m_tilesCacheSize; ++i){
        unlockTile(m_tilesCache[i].tile);
        unlockOldTile(m_tilesCache[i].oldtile);
        fetchTileDataForCache(m_tilesCache[i], m_column, m_topRow + i );
    }
}

std::int32_t KisVLineIterator2::x() const
{
    return m_x + m_offsetX;
}

std::int32_t KisVLineIterator2::y() const
{
    return m_y + m_offsetY;
}
