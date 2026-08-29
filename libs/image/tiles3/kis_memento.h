/*
 *  SPDX-FileCopyrightText: 2005 C. Boemann <cbo@boemann.dk>
 *            (c) 2009 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_MEMENTO_H_
#define KIS_MEMENTO_H_

#include <cstdint>

#include <PkRect.h>
#include <PkMutex.h>

#include "kis_global.h"

#include <kis_shared.h>
#include <kis_shared_ptr.h>


class KisMementoManager;

class KisMemento;
typedef KisSharedPtr<KisMemento> KisMementoSP;


class KisMemento : public KisShared
{
public:
    inline KisMemento(KisMementoManager* /*mementoManager*/) {
        m_extentMinX = qint32_MAX;
        m_extentMinY = qint32_MAX;
        m_extentMaxX = qint32_MIN;
        m_extentMaxY = qint32_MIN;

        m_oldDefaultPixel = 0;
        m_newDefaultPixel = 0;
    }

    inline ~KisMemento() {
        delete[] m_oldDefaultPixel;
        delete[] m_newDefaultPixel;
    }

    inline void extent(std::int32_t &x, std::int32_t &y, std::int32_t &w, std::int32_t &h) {
        const bool extentIsValid =
            m_extentMaxX >= m_extentMinX && m_extentMaxY >= m_extentMinY;

        if (extentIsValid) {
            x = m_extentMinX;
            y = m_extentMinY;
            w = m_extentMaxX - m_extentMinX + 1;
            h = m_extentMaxY - m_extentMinY + 1;
        } else {
            x = 0;
            y = 0;
            w = 0;
            h = 0;
        }
    }

    inline PkRect extent() {
        std::int32_t x, y, w, h;
        extent(x, y, w, h);
        return PkRect(x, y, w, h);
    }

    void saveOldDefaultPixel(const std::uint8_t* pixel, std::uint32_t pixelSize) {
        m_oldDefaultPixel = new std::uint8_t[pixelSize];
        memcpy(m_oldDefaultPixel, pixel, pixelSize);
    }

    void saveNewDefaultPixel(const std::uint8_t* pixel, std::uint32_t pixelSize) {
        m_newDefaultPixel = new std::uint8_t[pixelSize];
        memcpy(m_newDefaultPixel, pixel, pixelSize);
    }

    const std::uint8_t* oldDefaultPixel() const {
        return m_oldDefaultPixel;
    }

    const std::uint8_t* newDefaultPixel() const {
        return m_newDefaultPixel;
    }

private:
    friend class KisMementoManager;

    inline void updateExtent(std::int32_t col, std::int32_t row, PkMutex *currentMementoExtentLock) {
        const std::int32_t tileMinX = col * KisTileData::WIDTH;
        const std::int32_t tileMinY = row * KisTileData::HEIGHT;
        const std::int32_t tileMaxX = tileMinX + KisTileData::WIDTH - 1;
        const std::int32_t tileMaxY = tileMinY + KisTileData::HEIGHT - 1;

        {
            /**
             * HACK ALERT: the lock is stored in the memento
             * manager to avoid too many locks to be created.
             * Anyway, a memento manager can have only one
             * "current memento". And it would not be nice to
             * do KisTileData::WIDTH/HEIGHT multiplication
             * under the lock held.
             */
            PkMutexLocker l(currentMementoExtentLock);
            m_extentMinX = qMin(m_extentMinX, tileMinX);
            m_extentMaxX = qMax(m_extentMaxX, tileMaxX);
            m_extentMinY = qMin(m_extentMinY, tileMinY);
            m_extentMaxY = qMax(m_extentMaxY, tileMaxY);
        }
    }

private:
    std::uint8_t *m_oldDefaultPixel;
    std::uint8_t *m_newDefaultPixel;

    std::int32_t m_extentMinX;
    std::int32_t m_extentMaxX;
    std::int32_t m_extentMinY;
    std::int32_t m_extentMaxY;
};

#endif // KIS_MEMENTO_H_
