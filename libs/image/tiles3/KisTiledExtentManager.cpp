/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *  SPDX-FileCopyrightText: 2018 Andrey Kamakin <a.kamakin@icloud.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <cstdint>

#include "KisTiledExtentManager.h"

#include <PkVector.h>
#include "kis_tile_data_interface.h"
#include "kis_assert.h"
#include "kis_global.h"
#include "kis_debug.h"

KisTiledExtentManager::Data::Data()
    : m_min(qint32_MAX), m_max(qint32_MIN), m_count(0)
{
    PkWriteLocker lock(&m_migrationLock);
    m_capacity = InitialBufferSize;
    m_offset = 1;
    m_buffer = new PkAtomicInt[m_capacity];
}

KisTiledExtentManager::Data::~Data()
{
    PkWriteLocker lock(&m_migrationLock);
    delete[] m_buffer;
}

bool KisTiledExtentManager::Data::add(std::int32_t index)
{
    PkReadLocker lock(&m_migrationLock);
    std::int32_t currentIndex = m_offset + index;

    if (currentIndex < 0 || currentIndex >= m_capacity) {
        lock.unlock();
        migrate(index);
        lock.relock();
        currentIndex = m_offset + index;
    }

    KIS_ASSERT_RECOVER_NOOP(m_buffer[currentIndex].loadAcquire() >= 0);
    bool needsUpdateExtent = false;

    while (true) {
        PkReadLocker rl(&m_extentLock);

        int oldValue = m_buffer[currentIndex].loadAcquire();
        if (oldValue == 0) {
            rl.unlock();
            PkWriteLocker wl(&m_extentLock);

            if ((oldValue = m_buffer[currentIndex].loadAcquire()) == 0) {

                if (m_min > index) m_min = index;
                if (m_max < index) m_max = index;

                ++m_count;
                needsUpdateExtent = true;

                m_buffer[currentIndex].storeRelease(1);
            } else {
                m_buffer[currentIndex].storeRelease(oldValue + 1);
            }

            break;
        } else if (m_buffer[currentIndex].testAndSetOrdered(oldValue, oldValue + 1)) {
            break;
        }
    }

    return needsUpdateExtent;
}

bool KisTiledExtentManager::Data::remove(std::int32_t index)
{
    PkReadLocker lock(&m_migrationLock);
    std::int32_t currentIndex = m_offset + index;

    bool needsUpdateExtent = false;
    PkReadLocker rl(&m_extentLock);

    const int oldValue = m_buffer[currentIndex].fetchAndAddAcquire(-1);

    /**
     * That is not the droid you're looking for. If you see this assert
     * in the backtrace, most probably, the bug is not here. The crash
     * happens because two threads are trying to do device->clear(rc)
     * concurrently for the overlapping rects. That is, they are trying
     * to remove the same tile. Look higher!
     */
    KIS_SAFE_ASSERT_RECOVER(oldValue > 0) {
        m_buffer[currentIndex].storeRelaxed(0);
        return false;
    }

    if (oldValue == 1) {
        rl.unlock();
        PkWriteLocker wl(&m_extentLock);

        if (m_min == index) updateMin();
        if (m_max == index) updateMax();

        --m_count;
        needsUpdateExtent = true;
    }

    return needsUpdateExtent;
}

void KisTiledExtentManager::Data::replace(const PkVector<std::int32_t> &indexes)
{
    PkWriteLocker lock(&m_migrationLock);
    PkWriteLocker l(&m_extentLock);

    for (std::int32_t i = 0; i < m_capacity; ++i) {
        m_buffer[i].storeRelaxed(0);
    }

    m_min = qint32_MAX;
    m_max = qint32_MIN;
    m_count = 0;

    for (const std::int32_t index : indexes) {
        unsafeAdd(index);
    }
}

void KisTiledExtentManager::Data::clear()
{
    PkWriteLocker lock(&m_migrationLock);
    PkWriteLocker l(&m_extentLock);

    for (std::int32_t i = 0; i < m_capacity; ++i) {
        m_buffer[i].storeRelaxed(0);
    }

    m_min = qint32_MAX;
    m_max = qint32_MIN;
    m_count = 0;
}

bool KisTiledExtentManager::Data::isEmpty()
{
    return m_count == 0;
}

std::int32_t KisTiledExtentManager::Data::min()
{
    return m_min;
}

std::int32_t KisTiledExtentManager::Data::max()
{
    return m_max;
}

void KisTiledExtentManager::Data::unsafeAdd(std::int32_t index)
{
    std::int32_t currentIndex = m_offset + index;

    if (currentIndex < 0 || currentIndex >= m_capacity) {
        unsafeMigrate(index);
        currentIndex = m_offset + index;
    }

    if (!m_buffer[currentIndex].fetchAndAddRelaxed(1)) {
        if (m_min > index) m_min = index;
        if (m_max < index) m_max = index;
        ++m_count;
    }
}

void KisTiledExtentManager::Data::unsafeMigrate(std::int32_t index)
{
    std::int32_t oldCapacity = m_capacity;
    std::int32_t oldOffset = m_offset;
    std::int32_t currentIndex = m_offset + index;

    while (currentIndex < 0 || currentIndex >= m_capacity) {
        m_capacity <<= 1;

        if (currentIndex < 0) {
            m_offset <<= 1;
            currentIndex = m_offset + index;
        }
    }

    if (m_capacity != oldCapacity) {
        PkAtomicInt *newBuffer = new PkAtomicInt[m_capacity];
        std::int32_t start = m_offset - oldOffset;

        for (std::int32_t i = 0; i < oldCapacity; ++i) {
            newBuffer[start + i].storeRelaxed(m_buffer[i].loadRelaxed());
        }

        delete[] m_buffer;
        m_buffer = newBuffer;
    }
}

void KisTiledExtentManager::Data::migrate(std::int32_t index)
{
    PkWriteLocker lock(&m_migrationLock);
    unsafeMigrate(index);
}

void KisTiledExtentManager::Data::updateMin()
{
    KIS_SAFE_ASSERT_RECOVER_NOOP(m_min != qint32_MAX);

    std::int32_t start = m_min + m_offset;

    for (std::int32_t i = start; i < m_capacity; ++i) {
        std::int32_t current = m_buffer[i].loadRelaxed();

        if (current > 0) {
            m_min = i - m_offset;
            return;
        }
    }

    m_min = qint32_MAX;
}

void KisTiledExtentManager::Data::updateMax()
{
    KIS_SAFE_ASSERT_RECOVER_NOOP(m_min != qint32_MIN);

    std::int32_t start = m_max + m_offset;

    for (std::int32_t i = start; i >= 0; --i) {
        std::int32_t current = m_buffer[i].loadRelaxed();

        if (current > 0) {
            m_max = i - m_offset;
            return;
        }
    }

    m_max = qint32_MIN;
}

KisTiledExtentManager::KisTiledExtentManager()
{
    PkWriteLocker l(&m_extentLock);
    m_currentExtent = PkRect(0, 0, 0, 0);
}

void KisTiledExtentManager::notifyTileAdded(std::int32_t col, std::int32_t row)
{
    bool needsUpdateExtent = false;

    needsUpdateExtent |= m_colsData.add(col);
    needsUpdateExtent |= m_rowsData.add(row);

    if (needsUpdateExtent) {
        updateExtent();
    }
}

void KisTiledExtentManager::notifyTileRemoved(std::int32_t col, std::int32_t row)
{
    bool needsUpdateExtent = false;

    needsUpdateExtent |= m_colsData.remove(col);
    needsUpdateExtent |= m_rowsData.remove(row);

    if (needsUpdateExtent) {
        updateExtent();
    }
}

void KisTiledExtentManager::replaceTileStats(const PkVector<PkPoint> &indexes)
{
    PkVector<std::int32_t> colsIndexes;
    PkVector<std::int32_t> rowsIndexes;

    for (const PkPoint &index : indexes) {
        colsIndexes.append(index.x());
        rowsIndexes.append(index.y());
    }

    m_colsData.replace(colsIndexes);
    m_rowsData.replace(rowsIndexes);
    updateExtent();
}

void KisTiledExtentManager::clear()
{
    m_colsData.clear();
    m_rowsData.clear();

    PkWriteLocker lock(&m_extentLock);
    m_currentExtent = PkRect(0, 0, 0, 0);
}

PkRect KisTiledExtentManager::extent() const
{
    PkReadLocker lock(&m_extentLock);
    return m_currentExtent;
}

void KisTiledExtentManager::updateExtent()
{
    std::int32_t minX, width, minY, height;

    {
        PkReadLocker cl(&m_colsData.m_extentLock);

        if (m_colsData.isEmpty()) {
            minX = 0;
            width = 0;
        } else {
            minX = m_colsData.min() * KisTileData::WIDTH;
            width = (m_colsData.max() + 1) * KisTileData::WIDTH - minX;
        }
    }

    {
        PkReadLocker rl(&m_rowsData.m_extentLock);

        if (m_rowsData.isEmpty()) {
            minY = 0;
            height = 0;
        } else {
            minY = m_rowsData.min() * KisTileData::HEIGHT;
            height = (m_rowsData.max() + 1) * KisTileData::HEIGHT - minY;
        }
    }

    PkWriteLocker lock(&m_extentLock);
    m_currentExtent = PkRect(minX, minY, width, height);
}
