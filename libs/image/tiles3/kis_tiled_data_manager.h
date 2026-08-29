/*
 *  SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2009 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_TILEDDATAMANAGER_H_
#define KIS_TILEDDATAMANAGER_H_

#include <cstdint>

#include <PkVector.h>
#include <PkReadWriteLock.h>
#include <KisRegion.h>

#include <kis_shared.h>
#include <kis_shared_ptr.h>
#include "config-hash-table-implementation.h"

//#include "kis_debug.h"
#include "kritaimage_export.h"

#ifdef USE_LOCK_FREE_HASH_TABLE
#include "kis_tile_hash_table2.h"
#else
#include "kis_tile_hash_table.h"
#endif // USE_LOCK_FREE_HASH_TABLE

#include "kis_memento_manager.h"
#include "kis_memento.h"
#include "KisTiledExtentManager.h"

class KisTiledDataManager;
typedef KisSharedPtr<KisTiledDataManager> KisTiledDataManagerSP;

class KisTiledIterator;
class KisTiledRandomAccessor;
class KisPaintDeviceWriter;
class PkStream;

/**
 * KisTiledDataManager implements the interface that KisDataManager defines
 *
 * The interface definition is enforced by KisDataManager calling all the methods
 * which must also be defined in KisTiledDataManager. It is not allowed to change the interface
 * as other datamanagers may also rely on the same interface.
 *
 * * Storing undo/redo data
 * * Offering ordered and unordered iterators over rects of pixels
 * * (eventually) efficiently loading and saving data in a format
 * that may allow deferred loading.
 *
 * A datamanager knows nothing about the type of pixel data except
 * how many std::uint8_t's a single pixel takes.
 */

class KRITAIMAGE_EXPORT KisTiledDataManager : public KisShared
{
private:
    static const std::int32_t LEGACY_VERSION = 1;
    static const std::int32_t CURRENT_VERSION = 2;

protected:
    /*FIXME:*/
public:
    KisTiledDataManager(std::uint32_t pixelSize, const std::uint8_t *defPixel);
    virtual ~KisTiledDataManager();
    KisTiledDataManager(const KisTiledDataManager &dm);
    KisTiledDataManager & operator=(const KisTiledDataManager &dm);


protected:
    // Allow the baseclass of iterators access to the interior
    // derived iterator classes must go through KisTiledIterator
    friend class KisTiledIterator;
    friend class KisBaseIterator;
    friend class KisTiledRandomAccessor;
    friend class KisRandomAccessor2;
    friend class KisStressJob;

public:
    void setDefaultPixel(const std::uint8_t *defPixel);
    const std::uint8_t *defaultPixel() const {
        return m_defaultPixel;
    }

    /**
     * Every iterator fetches both types of tiles all the time: old and new.
     * For projection devices these tiles are **always** the same, but doing
     * two distinct calls makes double pressure on the read-write lock in the
     * hash table.
     *
     * Merging two calls into one allows us to avoid additional tile fetch from
     * the hash table and therefore reduce waiting time.
     */
    inline void getTilesPair(std::int32_t col, std::int32_t row, bool writable, KisTileSP *tile, KisTileSP *oldTile) {
        *tile = getTile(col, row, writable);

        bool unused;
        *oldTile = m_mementoManager->getCommittedTile(col, row, unused);

        if (!*oldTile) {
            *oldTile = *tile;
        }
    }

    inline KisTileSP getTile(std::int32_t col, std::int32_t row, bool writable) {
        if (writable) {
            bool newTile;
            KisTileSP tile = m_hashTable->getTileLazy(col, row, newTile);
            if (newTile) {
                m_extentManager.notifyTileAdded(col, row);
            }
            return tile;

        } else {
            bool unused;
            return m_hashTable->getReadOnlyTileLazy(col, row, unused);
        }
    }

    inline KisTileSP getReadOnlyTileLazy(std::int32_t col, std::int32_t row, bool &existingTile) {
        return m_hashTable->getReadOnlyTileLazy(col, row, existingTile);
    }

    inline KisTileSP getOldTile(std::int32_t col, std::int32_t row, bool &existingTile) {
        KisTileSP tile = m_mementoManager->getCommittedTile(col, row, existingTile);
        return tile ? tile : getReadOnlyTileLazy(col, row, existingTile);
    }

    inline KisTileSP getOldTile(std::int32_t col, std::int32_t row) {
        bool unused;
        return getOldTile(col, row, unused);
    }

    KisMementoSP getMemento() {
        PkWriteLocker locker(&m_lock);
        KisMementoSP memento = m_mementoManager->getMemento();
        memento->saveOldDefaultPixel(m_defaultPixel, m_pixelSize);
        return memento;
    }

    /**
     * Finishes having already started transaction
     */
    void commit() {
        PkWriteLocker locker(&m_lock);

        KisMementoSP memento = m_mementoManager->currentMemento();
        if(memento) {
            memento->saveNewDefaultPixel(m_defaultPixel, m_pixelSize);
        }

        m_mementoManager->commit();
    }

    void rollback(KisMementoSP memento) {
        commit();

        PkWriteLocker locker(&m_lock);
        m_mementoManager->rollback(m_hashTable, memento);
        const std::uint8_t *defaultPixel = memento->oldDefaultPixel();
        if(memcmp(m_defaultPixel, defaultPixel, m_pixelSize)) {
            setDefaultPixelImpl(defaultPixel);
        }
        recalculateExtent();
    }
    void rollforward(KisMementoSP memento) {
        commit();

        PkWriteLocker locker(&m_lock);
        m_mementoManager->rollforward(m_hashTable, memento);
        const std::uint8_t *defaultPixel = memento->newDefaultPixel();
        if(memcmp(m_defaultPixel, defaultPixel, m_pixelSize)) {
            setDefaultPixelImpl(defaultPixel);
        }
        recalculateExtent();
    }
    bool hasCurrentMemento() const {
        return m_mementoManager->hasCurrentMemento();
        //return true;
    }

    /**
     * Removes all the history that precedes the revision
     * pointed by oldestMemento. That is after calling to
     * purgeHistory(someMemento) you won't be able to do
     * rollback(someMemento) anymore.
     */
    void purgeHistory(KisMementoSP oldestMemento) {
        PkWriteLocker locker(&m_lock);
        m_mementoManager->purgeHistory(oldestMemento);
    }

    static void releaseInternalPools();

protected:
    /**
     * Reads and writes the tiles
     */
    bool write(KisPaintDeviceWriter &store);
    bool read(PkStream *stream);

    void purge(const PkRect& area);

    inline std::uint32_t pixelSize() const {
        return m_pixelSize;
    }

    /* FIXME:*/
public:


    void  extent(std::int32_t &x, std::int32_t &y, std::int32_t &w, std::int32_t &h) const;
    void  setExtent(std::int32_t x, std::int32_t y, std::int32_t w, std::int32_t h);
    PkRect extent() const;
    void  setExtent(PkRect newRect);

    KisRegion region() const;

    void clear(PkRect clearRect, std::uint8_t clearValue);
    void clear(PkRect clearRect, const std::uint8_t *clearPixel);
    void clear(std::int32_t x, std::int32_t y, std::int32_t w, std::int32_t h, std::uint8_t clearValue);
    void clear(std::int32_t x, std::int32_t y,  std::int32_t w, std::int32_t h, const std::uint8_t *clearPixel);
    void clear();

    /**
     * Clones rect from another datamanager. The cloned area will be
     * shared between both datamanagers as much as possible using
     * copy-on-write. Parts of the rect that cannot be shared
     * (cross tiles) are deep-copied,
     */
    void bitBlt(KisTiledDataManager *srcDM, const PkRect &rect);

    /**
     * The same as \ref bitBlt(), but reads old data
     */
    void bitBltOldData(KisTiledDataManager *srcDM, const PkRect &rect);

    /**
     * Clones rect from another datamanager in a rough and fast way.
     * All the tiles touched by rect will be shared, between both
     * managers, that means it will copy a bigger area than was
     * requested. This method is supposed to be used for bitBlt'ing
     * into temporary paint devices.
     */
    void bitBltRough(KisTiledDataManager *srcDM, const PkRect &rect);

    /**
     * The same as \ref bitBltRough(), but reads old data
     */
    void bitBltRoughOldData(KisTiledDataManager *srcDM, const PkRect &rect);

    /**
     * write the specified data to x, y. There is no checking on pixelSize!
     */
    void setPixel(std::int32_t x, std::int32_t y, const std::uint8_t * data);


    /**
     * Copy the bytes in the specified rect to a vector. The caller is responsible
     * for managing the vector.
     *
     * \param bytes the bytes
     * \param x x of top left corner
     * \param y y of top left corner
     * \param w width
     * \param h height
     * \param dataRowStride is the step (in bytes) which should be
     *                      added to \p bytes pointer to get to the
     *                      next row
     */
    void readBytes(std::uint8_t * bytes,
                   std::int32_t x, std::int32_t y,
                   std::int32_t w, std::int32_t h,
                   std::int32_t dataRowStride = -1) const;
    /**
     * Copy the bytes in the vector to the specified rect. If there are bytes left
     * in the vector after filling the rect, they will be ignored. If there are
     * not enough bytes, the rest of the rect will be filled with the default value
     * given (by default, 0);
     *
     * \param bytes the bytes
     * \param x x of top left corner
     * \param y y of top left corner
     * \param w width
     * \param h height
     * \param dataRowStride is the step (in bytes) which should be
     *                      added to \p bytes pointer to get to the
     *                      next row
     */
    void writeBytes(const std::uint8_t * bytes,
                    std::int32_t x, std::int32_t y,
                    std::int32_t w, std::int32_t h,
                    std::int32_t dataRowStride = -1);

    /**
     * Copy the bytes in the paint device into a vector of arrays of bytes,
     * where the number of arrays is the number of channels in the
     * paint device. If the specified area is larger than the paint
     * device's extent, the default pixel will be read.
     */
    PkVector<std::uint8_t*> readPlanarBytes(PkVector<std::int32_t> channelsizes, std::int32_t x, std::int32_t y, std::int32_t w, std::int32_t h) const;

    /**
     * Write the data in the separate arrays to the channels. If there
     * are less vectors than channels, the remaining channels will not
     * be copied. If any of the arrays points to 0, the channel in
     * that location will not be touched. If the specified area is
     * larger than the paint device, the paint device will be
     * extended. There are no guards: if the area covers more pixels
     * than there are bytes in the arrays, krita will happily fill
     * your paint device with areas of memory you never wanted to be
     * read. Krita may also crash.
     */
    void writePlanarBytes(PkVector<std::uint8_t*> planes, PkVector<std::int32_t> channelsizes, std::int32_t x, std::int32_t y, std::int32_t w, std::int32_t h);

    /**
     * Get the number of contiguous columns starting at x, valid for all values
     * of y between minY and maxY.
     */
    std::int32_t numContiguousColumns(std::int32_t x, std::int32_t minY, std::int32_t maxY) const;

    /**
     * Get the number of contiguous rows starting at y, valid for all values
     * of x between minX and maxX.
     */
    std::int32_t numContiguousRows(std::int32_t y, std::int32_t minX, std::int32_t maxX) const;

    /**
     * Get the row stride at pixel (x, y). This is the number of bytes to add to a
     * pointer to pixel (x, y) to access (x, y + 1).
     */
    std::int32_t rowStride(std::int32_t x, std::int32_t y) const;

private:
    KisTileHashTable *m_hashTable;
    KisMementoManager *m_mementoManager;
    std::uint8_t* m_defaultPixel;
    std::int32_t m_pixelSize;
    KisTiledExtentManager m_extentManager;

    mutable PkReadWriteLock m_lock;

private:
    // Allow compression routines to calculate (col,row) coordinates
    // and pixel size
    friend class KisAbstractTileCompressor;
    friend class KisTileDataWrapper;
    inline std::int32_t xToCol(std::int32_t x) const
    {
        return divideRoundDown(x, KisTileData::WIDTH);
    }
    inline std::int32_t yToRow(std::int32_t y) const
    {
        return divideRoundDown(y, KisTileData::HEIGHT);
    }

private:
    void setDefaultPixelImpl(const std::uint8_t *defPixel);

    bool writeTilesHeader(KisPaintDeviceWriter &store, std::uint32_t numTiles);
    bool processTilesHeader(PkStream *stream, std::uint32_t &numTiles);

    inline std::int32_t divideRoundDown(std::int32_t x, const std::int32_t y) const
    {
        /**
         * Equivalent to the following:
         * -(( -x + (y-1) ) / y)
         */

        return x >= 0 ? x / y : -(((-x - 1) / y) + 1);
    }

    void recalculateExtent();

    std::uint8_t* duplicatePixel(std::int32_t num, const std::uint8_t *pixel);

    template<bool useOldSrcData>
        void bitBltImpl(KisTiledDataManager *srcDM, const PkRect &rect);
    template<bool useOldSrcData>
        void bitBltRoughImpl(KisTiledDataManager *srcDM, const PkRect &rect);

    void writeBytesBody(const std::uint8_t *data,
                        std::int32_t x, std::int32_t y,
                        std::int32_t width, std::int32_t height,
                        std::int32_t dataRowStride = -1);
    void readBytesBody(std::uint8_t *data,
                       std::int32_t x, std::int32_t y,
                       std::int32_t width, std::int32_t height,
                       std::int32_t dataRowStride = -1) const;

    template <bool allChannelsPresent>
    void writePlanarBytesBody(PkVector<std::uint8_t*> planes,
                              PkVector<std::int32_t> channelsizes,
                              std::int32_t x, std::int32_t y, std::int32_t w, std::int32_t h);
    PkVector<std::uint8_t*> readPlanarBytesBody(PkVector<std::int32_t> channelsizes,
                                         std::int32_t x, std::int32_t y,
                                         std::int32_t w, std::int32_t h) const;
public:
    void debugPrintInfo() {
        m_mementoManager->debugPrintInfo();
    }

};

// during development the following line helps to check the interface is correct
// it should be safe to keep it here even during normal compilation
//#include "kis_datamanager.h"

#endif // KIS_TILEDDATAMANAGER_H_
