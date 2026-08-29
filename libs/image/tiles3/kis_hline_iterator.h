/* 
 *  SPDX-FileCopyrightText: 2010 Lukáš Tvrdý <lukast.dev@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _KIS_HLINE_ITERATOR_H_
#define _KIS_HLINE_ITERATOR_H_

#include <cstdint>

#include "kis_base_iterator.h"
#include "kritaimage_export.h"
#include "kis_iterator_ng.h"
#include <PkVector.h>

class KRITAIMAGE_EXPORT KisHLineIterator2 : public KisHLineIteratorNG, public KisBaseIterator {
    KisHLineIterator2(const KisHLineIterator2&);
    KisHLineIterator2& operator=(const KisHLineIterator2&);

public:
    struct KisTileInfo {
        KisTileSP tile;
        KisTileSP oldtile;
        std::uint8_t* data {nullptr};
        std::uint8_t* oldData {nullptr};
    };


public:    
    KisHLineIterator2(KisDataManager *dataManager, std::int32_t x, std::int32_t y, std::int32_t w, std::int32_t offsetX, std::int32_t offsetY, bool writable, KisIteratorCompleteListener *listener);
    ~KisHLineIterator2() override;
    
    bool nextPixel() override;
    void nextRow() override;
    const std::uint8_t* oldRawData() const override;
    const std::uint8_t* rawDataConst() const override;
    std::uint8_t* rawData() override;
    std::int32_t nConseqPixels() const override;
    bool nextPixels(std::int32_t n) override;
    std::int32_t x() const override;
    std::int32_t y() const override;

    void resetPixelPos() override;
    void resetRowPos() override;

private:
    std::int32_t m_offsetX {0};
    std::int32_t m_offsetY {0};

    std::int32_t m_x {0};        // current x position
    std::int32_t m_y {0};        // current y position
    std::int32_t m_row {0};    // current row in tilemgr
    std::uint32_t m_index {0};    // current col in tilemgr
    std::uint32_t m_tileWidth {0};
    std::uint8_t *m_data {nullptr};
    std::uint8_t *m_oldData {nullptr};
    bool m_havePixels {false};
    
    std::int32_t m_right {0};
    std::int32_t m_left {0};
    std::int32_t m_top {0};
    std::int32_t m_leftCol {0};
    std::int32_t m_rightCol {0};

    std::int32_t m_rightmostInTile {0}; // limited by the current tile border only

    std::int32_t m_leftInLeftmostTile {0};
    std::int32_t m_yInTile {0};

    PkVector<KisTileInfo> m_tilesCache;
    std::uint32_t m_tilesCacheSize {0};
    
private:

    void switchToTile(std::int32_t xInTile);
    void fetchTileDataForCache(KisTileInfo& kti, std::int32_t col, std::int32_t row);
    void preallocateTiles();
};
#endif
