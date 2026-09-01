/*
 *  SPDX-FileCopyrightText: 2019 Kuntal Majumder <hellozee@disroot.org>
 *
 *  SPDX-License-Identifier: LGPL-2.1-only
 */

#ifndef KISMAGNETICWORKER_H
#define KISMAGNETICWORKER_H

#include <kis_paint_device.h>
#include <kritaselectiontools_export.h>

struct KisMagneticGraph;
struct VertexDescriptor;

class KisMagneticLazyTiles {
private:
    PkVector<PkRect> m_tiles;
    PkVector<qreal> m_radiusRecord;
    KisPaintDeviceSP m_dev;
    PkSize m_tileSize;
    int m_tilesPerRow;

public:
    KisMagneticLazyTiles(KisPaintDeviceSP dev);
    void filter(qreal radius, PkRect &rect);
    inline KisPaintDeviceSP device(){ return m_dev; }
    inline PkVector<PkRect> tiles(){ return m_tiles; }
};

class KRITASELECTIONTOOLS_EXPORT KisMagneticWorker {
public:
    KisMagneticWorker(const KisPaintDeviceSP &dev);

    PkVector<PkPointF> computeEdge(int bounds, PkPoint start, PkPoint end, qreal radius);
    void saveTheImage(PkVector<PkPointF> points);
    qreal intensity(PkPoint pt);

private:
    KisMagneticLazyTiles m_lazyTileFilter;
    KisMagneticGraph *m_graph {nullptr};
};

KRITASELECTIONTOOLS_EXPORT double magneticEdgeWeight(
    KisMagneticGraph &graph,
    const VertexDescriptor &first,
    const VertexDescriptor &second);

#endif // ifndef KISMAGNETICWORKER_H
