/*
 *  SPDX-FileCopyrightText: 2010 Lukáš Tvrdý <lukast.dev@gmail.com>
 *  SPDX-FileCopyrightText: 2010 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _KIS_VLINE_ITERATOR_H_
#define _KIS_VLINE_ITERATOR_H_

#include <cstdint>

#include "kis_base_iterator.h"
#include "kritaimage_export.h"
#include "kis_iterator_ng.h"
#include <PkVector.h>

class KRITAIMAGE_EXPORT KisVLineIterator2 : public KisVLineIteratorNG, KisBaseIterator {
    KisVLineIterator2(const KisVLineIterator2&);
    KisVLineIterator2& operator=(const KisVLineIterator2&);

public:
    struct KisTileInfo {
        KisTileSP tile;
        KisTileSP oldtile;
        std::uint8_t* data {nullptr};
        std::uint8_t* oldData {nullptr};
    };


public:
    KisVLineIterator2(KisDataManager *dataManager, std::int32_t x, std::int32_t y, std::int32_t h, std::int32_t offsetX, std::int32_t offsetY, bool writable, KisIteratorCompleteListener *completeListener);
    ~KisVLineIterator2() override;

    void resetPixelPos() override;
    void resetColumnPos() override;

    bool nextPixel() override;
    void nextColumn() override;
    const std::uint8_t* rawDataConst() const override;
    const std::uint8_t* oldRawData() const override;
    std::uint8_t* rawData() override;
    std::int32_t nConseqPixels() const override;
    bool nextPixels(std::int32_t n) override;
    std::int32_t x() const override;
    std::int32_t y() const override;

private:
    std::int32_t m_offsetX {0};
    std::int32_t m_offsetY {0};

    std::int32_t m_x {0};        // current x position
    std::int32_t m_y {0};        // current y position
    std::int32_t m_column {0};    // current column in tilemgr
    std::int32_t m_index {0};    // current row in tilemgr
    std::int32_t m_tileSize {0};
    std::uint8_t *m_data {nullptr};
    std::uint8_t *m_dataBottom {nullptr};
    std::uint8_t *m_oldData {nullptr};
    bool m_havePixels {false};

    std::int32_t m_top {0};
    std::int32_t m_bottom {0};
    std::int32_t m_left {0};
    std::int32_t m_topRow {0};
    std::int32_t m_bottomRow {0};

    std::int32_t m_topInTopmostTile {0};
    std::int32_t m_xInTile {0};
    std::int32_t m_lineStride {0};

    PkVector<KisTileInfo> m_tilesCache;
    std::int32_t m_tilesCacheSize {0};

private:

    void switchToTile(std::int32_t xInTile);
    void fetchTileDataForCache(KisTileInfo& kti, std::int32_t col, std::int32_t row);
    void preallocateTiles();
};
#endif
