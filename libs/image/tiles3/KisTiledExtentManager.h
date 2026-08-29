/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *  SPDX-FileCopyrightText: 2018 Andrey Kamakin <a.kamakin@icloud.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISTILEDEXTENTMANAGER_H
#define KISTILEDEXTENTMANAGER_H

#include <cstdint>

#include <PkReadWriteLock.h>
#include <PkRect.h>
#include <PkPoint.h>
#include <PkVector.h>
#include <PkAtomic.h>
#include "kritaimage_export.h"


class KRITAIMAGE_EXPORT KisTiledExtentManager
{
    static const std::int32_t InitialBufferSize = 256;

    class KRITAIMAGE_EXPORT Data
    {
    public:
        Data();
        ~Data();

        bool add(std::int32_t index);
        bool remove(std::int32_t index);
        void replace(const PkVector<std::int32_t> &indexes);
        void clear();
        bool isEmpty();
        std::int32_t min();
        std::int32_t max();

    public:
        PkReadWriteLock m_extentLock;

    private:
        inline void unsafeAdd(std::int32_t index);
        inline void unsafeMigrate(std::int32_t index);
        inline void migrate(std::int32_t index);
        inline void updateMin();
        inline void updateMax();

    private:
        std::int32_t m_min;
        std::int32_t m_max;
        std::int32_t m_offset;
        std::int32_t m_capacity;
        std::int32_t m_count;
        PkAtomicInt *m_buffer;
        PkReadWriteLock m_migrationLock;
    };

public:
    KisTiledExtentManager();

    void notifyTileAdded(std::int32_t col, std::int32_t row);
    void notifyTileRemoved(std::int32_t col, std::int32_t row);
    void replaceTileStats(const PkVector<PkPoint> &indexes);
    void clear();
    PkRect extent() const;

private:
    void updateExtent();
    friend class KisTiledDataManagerTest;

private:
    mutable PkReadWriteLock m_extentLock;
    PkRect m_currentExtent;
    Data m_colsData;
    Data m_rowsData;
};

#endif // KISTILEDEXTENTMANAGER_H
