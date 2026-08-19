/*
 *  SPDX-FileCopyrightText: 2021 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisRectsGrid.h"
#include "kis_assert.h"
#include <QtCore/qmath.h>
#include "kis_lod_transform_base.h"
#include "kis_global.h"
#include "kis_algebra_2d.h"
#include <KisUsageLogger.h>

#include "kis_debug.h"

KisRectsGrid::KisRectsGrid(int gridSize)
    : m_gridSize(gridSize)
    , m_logGridSize(qFloor(std::log2(gridSize)))
{
    if (!qFuzzyCompare(std::log2(gridSize), qreal(m_logGridSize))) {
        KisUsageLogger::log(QString("Invalid grid configuration. Grid size: %1, log grid size: %2. Resetting to 64 and 6").arg(gridSize, m_logGridSize));
        m_gridSize = 64;
        m_logGridSize = 6;
    }
}

void KisRectsGrid::resize(const PkRect &newMappedAreaSize)
{
    KIS_SAFE_ASSERT_RECOVER_NOOP(m_mappedAreaSize.isEmpty() || newMappedAreaSize.contains(m_mappedAreaSize));

    QVector<quint8> newMapping(newMappedAreaSize.width() * newMappedAreaSize.height());

    const int xDiff = m_mappedAreaSize.x() - newMappedAreaSize.x();
    const int yDiff = m_mappedAreaSize.y() - newMappedAreaSize.y();

    int dstRowStride = newMappedAreaSize.width();
    int dstRowStart = xDiff + yDiff * dstRowStride;

    for (int y = 0; y < m_mappedAreaSize.height(); y++) {
        int dstRowIndex = dstRowStart + dstRowStride * y;
        int srcRowIndex = m_mappedAreaSize.width() * y;

        memcpy(&newMapping[dstRowIndex], &m_mapping[srcRowIndex], m_mappedAreaSize.width());
    }

    std::swap(newMapping, m_mapping);
    m_mappedAreaSize = newMappedAreaSize;
}

PkRect KisRectsGrid::alignRect(const PkRect &rc) const
{
    return KisLodTransformBase::alignedRect(rc, m_logGridSize);
}

QVector<PkRect> KisRectsGrid::addRect(const PkRect &rc)
{
    return addAlignedRect(alignRect(rc));
}

QVector<PkRect> KisRectsGrid::addAlignedRect(const PkRect &rc)
{
    if (rc.isEmpty()) return QVector<PkRect>();

    const PkRect mappedRect = KisLodTransformBase::scaledRect(rc, m_logGridSize);

    if (!m_mappedAreaSize.contains(mappedRect)) {
        PkRect nextMappingSize = m_mappedAreaSize | mappedRect;
        nextMappingSize = KisAlgebra2D::blowRect(nextMappingSize, 0.2);
        resize(nextMappingSize);
    }

    QVector<PkRect> addedRects;

    for (int y = mappedRect.y(); y <= mappedRect.bottom(); y++) {
        for (int x = mappedRect.x(); x <= mappedRect.right(); x++) {
            quint8 *ptr = &m_mapping[m_mappedAreaSize.width() * (y - m_mappedAreaSize.y()) + (x - m_mappedAreaSize.x())];
            if (!*ptr) {
                *ptr = 1;
                addedRects.append(KisLodTransformBase::upscaledRect(PkRect(x, y, 1, 1), m_logGridSize));
            }
        }
    }
    return addedRects;
}

inline PkRect KisRectsGrid::shrinkRectToAlignedGrid(const PkRect &srcRect, int lod)
{
    qint32 alignment = 1 << lod;

    qint32 x1, y1, x2, y2;
    srcRect.getCoords(&x1, &y1, &x2, &y2);

    x1--;
    y1--;
    x2++;
    y2++;

    KisLodTransformBase::alignByPow2ButOneHi(x1, alignment);
    KisLodTransformBase::alignByPow2ButOneHi(y1, alignment);

    KisLodTransformBase::alignByPow2Lo(x2, alignment);
    KisLodTransformBase::alignByPow2Lo(y2, alignment);

    x1++;
    y1++;
    x2--;
    y2--;

    PkRect rect;
    rect.setCoords(x1, y1, x2, y2);

    return rect;
}

QVector<PkRect> KisRectsGrid::removeRect(const PkRect &rc)
{
    const PkRect alignedRect = shrinkRectToAlignedGrid(rc, m_logGridSize);
    return !alignedRect.isEmpty() ? removeAlignedRect(alignedRect) : QVector<PkRect>();
}

QVector<PkRect> KisRectsGrid::removeAlignedRect(const PkRect &rc)
{
    const PkRect mappedRect = KisLodTransformBase::scaledRect(rc, m_logGridSize);

    // NOTE: we never shrink the size of the grid, just keep it as big as
    //       it ever was

    QVector<PkRect> removedRects;

    for (int y = mappedRect.y(); y <= mappedRect.bottom(); y++) {
        for (int x = mappedRect.x(); x <= mappedRect.right(); x++) {
            quint8 *ptr = &m_mapping[m_mappedAreaSize.width() * (y - m_mappedAreaSize.y()) + (x - m_mappedAreaSize.x())];
            if (*ptr) {
                *ptr = 0;
                removedRects.append(KisLodTransformBase::upscaledRect(PkRect(x, y, 1, 1), m_logGridSize));
            }
        }
    }
    return removedRects;
}

bool KisRectsGrid::contains(const PkRect &rc) const
{
    const PkRect mappedRect = KisLodTransformBase::scaledRect(alignRect(rc), m_logGridSize);

    if (!m_mappedAreaSize.contains(mappedRect)) return false;

    for (int y = mappedRect.y(); y <= mappedRect.bottom(); y++) {
        for (int x = mappedRect.x(); x <= mappedRect.right(); x++) {
            const quint8 *ptr = &m_mapping[m_mappedAreaSize.width() * (y - m_mappedAreaSize.y()) + (x - m_mappedAreaSize.x())];
            if (!*ptr) return false;
        }
    }

    return true;
}

PkRect KisRectsGrid::boundingRect() const {
    PkRect gridBounds;

    for (int y = m_mappedAreaSize.y(); y <= m_mappedAreaSize.bottom(); y++) {
        for (int x = m_mappedAreaSize.x(); x <= m_mappedAreaSize.right(); x++) {
            gridBounds |= PkRect(x, y, 1, 1);
        }
    }

    return KisLodTransformBase::upscaledRect(gridBounds, m_logGridSize);
}
