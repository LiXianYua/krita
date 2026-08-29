/*
 *  SPDX-FileCopyrightText: 2004 C. Boemann <cbo@boemann.dk>
 *  SPDX-FileCopyrightText: 2009 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <PkString.h>
#include "kis_debug.h"
#include "kis_global.h"

//#define SHARED_TILES_SANITY_CHECK

#include <cstdint>
#include <cassert>
#include <cstdlib>
#include <cstdio>

#ifndef PK_TILES_ASSERT
#  ifdef assert
#    undef assert
#  endif
#  if defined(QT_NO_DEBUG) || defined(NDEBUG)
#    define PK_TILES_ASSERT(condition) ((void)sizeof(condition))
#    define PK_TILES_ASSERT_X(condition, where, what) \
        ((void)sizeof(condition), (void)sizeof(where), (void)sizeof(what))
#  else
#    define PK_TILES_ASSERT(condition) \
        ((condition) ? static_cast<void>(0) : std::abort())
#    define PK_TILES_ASSERT_X(condition, where, what) \
        ((condition) ? static_cast<void>(0) : \
         (std::fprintf(stderr, "%s: %s\n", (where), (what)), std::abort()))
#  endif
#  define assert PK_TILES_ASSERT
#endif


template<class T>
KisTileHashTableTraits<T>::KisTileHashTableTraits(KisMementoManager *mm)
        : m_lock(PkReadWriteLock::NonRecursive)
{
    m_hashTable = new TileTypeSP [TABLE_SIZE];

    m_numTiles = 0;
    m_defaultTileData = 0;
    m_mementoManager = mm;
}

template<class T>
KisTileHashTableTraits<T>::KisTileHashTableTraits(const KisTileHashTableTraits<T> &ht,
        KisMementoManager *mm)
        : m_lock(PkReadWriteLock::NonRecursive)
{
    PkReadLocker locker(&ht.m_lock);

    m_mementoManager = mm;
    m_defaultTileData = 0;
    setDefaultTileDataImp(ht.m_defaultTileData);

    m_hashTable = new TileTypeSP [TABLE_SIZE];


    TileTypeSP foreignTile;
    TileTypeSP nativeTile;
    TileTypeSP nativeTileHead;
    for (std::int32_t i = 0; i < TABLE_SIZE; i++) {
        nativeTileHead = 0;

        foreignTile = ht.m_hashTable[i];
        while (foreignTile) {
            nativeTile = TileTypeSP(new TileType(*foreignTile, m_mementoManager));
            nativeTile->setNext(nativeTileHead);
            nativeTileHead = nativeTile;

            foreignTile = foreignTile->next();
        }

        m_hashTable[i] = nativeTileHead;
    }
    m_numTiles = ht.m_numTiles;
}

template<class T>
KisTileHashTableTraits<T>::~KisTileHashTableTraits()
{
    clear();
    delete[] m_hashTable;
    setDefaultTileDataImp(0);
}

template<class T>
std::uint32_t KisTileHashTableTraits<T>::calculateHash(std::int32_t col, std::int32_t row)
{
    return ((row << 5) + (col & 0x1F)) & 0x3FF;
}

template<class T>
typename KisTileHashTableTraits<T>::TileTypeSP
KisTileHashTableTraits<T>::getTileMinefieldWalk(std::int32_t col, std::int32_t row, std::int32_t idx)
{
    // WARNING: this function is here only for educational purposes! Don't
    //          use it! It causes race condition in a shared pointer copy-ctor
    //          when accessing m_hashTable!

    /**
     * This is a special method for dangerous and unsafe access to
     * the tiles table. Thanks to the fact that our shared pointers
     * are thread safe, we can iterate through the linked list without
     * having any locks help. In the worst case, we will miss the needed
     * tile. In that case, the higher level code will do the proper
     * locking and do the second try with all the needed locks held.
     */

    TileTypeSP headTile = m_hashTable[idx];
    TileTypeSP tile = headTile;

    for (; tile; tile = tile->next()) {
        if (tile->col() == col &&
            tile->row() == row) {

            if (m_hashTable[idx] != headTile) {
                tile.clear();
            }

            break;
        }
    }

    return tile;
}

template<class T>
typename KisTileHashTableTraits<T>::TileTypeSP
KisTileHashTableTraits<T>::getTile(std::int32_t col, std::int32_t row, std::int32_t idx)
{
    TileTypeSP tile = m_hashTable[idx];

    for (; tile; tile = tile->next()) {
        if (tile->col() == col &&
                tile->row() == row) {

            return tile;
        }
    }

    return TileTypeSP();
}

template<class T>
void KisTileHashTableTraits<T>::linkTile(TileTypeSP tile, std::int32_t idx)
{
    TileTypeSP firstTile = m_hashTable[idx];

#ifdef SHARED_TILES_SANITY_CHECK
    PK_TILES_ASSERT_X(!tile->next(), "KisTileHashTableTraits<T>::linkTile",
                      "A tile can't be shared by several hash tables, sorry.");
#endif

    tile->setNext(firstTile);
    m_hashTable[idx] = tile;
    m_numTiles++;
}

template<class T>
bool KisTileHashTableTraits<T>::unlinkTile(std::int32_t col, std::int32_t row, std::int32_t idx)
{
    TileTypeSP tile = m_hashTable[idx];
    TileTypeSP prevTile;

    for (; tile; tile = tile->next()) {
        if (tile->col() == col &&
                tile->row() == row) {

            if (prevTile)
                prevTile->setNext(tile->next());
            else
                /* optimize here*/
                m_hashTable[idx] = tile->next();

            /**
             * The shared pointer may still be accessed by someone, so
             * we need to disconnects the tile from memento manager
             * explicitly
             */
            tile->setNext(TileTypeSP());
            tile->notifyDetachedFromDataManager();
            tile.clear();

            m_numTiles--;
            return true;
        }
        prevTile = tile;
    }

    return false;
}

template<class T>
inline void KisTileHashTableTraits<T>::setDefaultTileDataImp(KisTileData *defaultTileData)
{
    if (m_defaultTileData) {
        m_defaultTileData->release();
        m_defaultTileData = 0;
    }

    if (defaultTileData) {
        defaultTileData->acquire();
        m_defaultTileData = defaultTileData;
    }
}

template<class T>
inline KisTileData* KisTileHashTableTraits<T>::defaultTileDataImp() const
{
    return m_defaultTileData;
}


template<class T>
bool KisTileHashTableTraits<T>::tileExists(std::int32_t col, std::int32_t row)
{
    return this->getExistingTile(col, row);
}

template<class T>
typename KisTileHashTableTraits<T>::TileTypeSP
KisTileHashTableTraits<T>::getExistingTile(std::int32_t col, std::int32_t row)
{
    const std::int32_t idx = calculateHash(col, row);

    // NOTE: minefield walk is disabled due to supposed unsafety,
    //       see bug 391270

    PkReadLocker locker(&m_lock);
    return getTile(col, row, idx);
}

template<class T>
typename KisTileHashTableTraits<T>::TileTypeSP
KisTileHashTableTraits<T>::getTileLazy(std::int32_t col, std::int32_t row,
                                       bool& newTile)
{
    const std::int32_t idx = calculateHash(col, row);

    // NOTE: minefield walk is disabled due to supposed unsafety,
    //       see bug 391270

    newTile = false;
    TileTypeSP tile;

    {
        PkReadLocker locker(&m_lock);
        tile = getTile(col, row, idx);
    }

    if (!tile) {
        PkWriteLocker locker(&m_lock);
        tile = new TileType(col, row, m_defaultTileData, m_mementoManager);
        linkTile(tile, idx);
        newTile = true;
    }

    return tile;
}

template<class T>
typename KisTileHashTableTraits<T>::TileTypeSP
KisTileHashTableTraits<T>::getReadOnlyTileLazy(std::int32_t col, std::int32_t row, bool &existingTile)
{
    const std::int32_t idx = calculateHash(col, row);

    // NOTE: minefield walk is disabled due to supposed unsafety,
    //       see bug 391270

    PkReadLocker locker(&m_lock);

    TileTypeSP tile = getTile(col, row, idx);
    existingTile = tile;

    if (!existingTile) {
        tile = new TileType(col, row, m_defaultTileData, 0);
    }

    return tile;
}

template<class T>
void KisTileHashTableTraits<T>::addTile(TileTypeSP tile)
{
    const std::int32_t idx = calculateHash(tile->col(), tile->row());

    PkWriteLocker locker(&m_lock);
    linkTile(tile, idx);
}

template<class T>
bool KisTileHashTableTraits<T>::deleteTile(std::int32_t col, std::int32_t row)
{
    const std::int32_t idx = calculateHash(col, row);

    PkWriteLocker locker(&m_lock);
    return unlinkTile(col, row, idx);
}

template<class T>
bool KisTileHashTableTraits<T>::deleteTile(TileTypeSP tile)
{
    return deleteTile(tile->col(), tile->row());
}

template<class T>
void KisTileHashTableTraits<T>::clear()
{
    PkWriteLocker locker(&m_lock);
    TileTypeSP tile = TileTypeSP();
    std::int32_t i;

    for (i = 0; i < TABLE_SIZE; i++) {
        tile = m_hashTable[i];

        while (tile) {
            TileTypeSP tmp = tile;
            tile = tile->next();

            /**
             * About disconnection of tiles see a comment in unlinkTile()
             */

            tmp->setNext(TileTypeSP());
            tmp->notifyDetachedFromDataManager();
            tmp = 0;

            m_numTiles--;
        }

        m_hashTable[i] = 0;
    }

    assert(!m_numTiles);
}

template<class T>
void KisTileHashTableTraits<T>::setDefaultTileData(KisTileData *defaultTileData)
{
    PkWriteLocker locker(&m_lock);
    setDefaultTileDataImp(defaultTileData);
}

template<class T>
KisTileData* KisTileHashTableTraits<T>::defaultTileData() const
{
    PkWriteLocker locker(&m_lock);
    return defaultTileDataImp();
}

template <class T>
inline KisTileData* KisTileHashTableTraits<T>::refAndFetchDefaultTileData() const
{
    PkWriteLocker locker(&m_lock);
    KisTileData *result = defaultTileDataImp();
    result->ref();
    return result;
}


/*************** Debugging stuff ***************/

template<class T>
void KisTileHashTableTraits<T>::debugPrintInfo()
{
    if (!m_numTiles) return;

    infoTiles << "==========================\n"
             << "TileHashTable:"
             << "\n   def. data:\t\t" << m_defaultTileData
             << "\n   numTiles:\t\t" << m_numTiles;
    debugListLengthDistribution();
    infoTiles << "==========================\n";
}

template<class T>
std::int32_t KisTileHashTableTraits<T>::debugChainLen(std::int32_t idx)
{
    std::int32_t len = 0;
    for (TileTypeSP it = m_hashTable[idx]; it; it = it->next(), len++) ;
    return len;
}

template<class T>
void KisTileHashTableTraits<T>::debugMaxListLength(std::int32_t &min, std::int32_t &max)
{
    TileTypeSP tile;
    std::int32_t maxLen = 0;
    std::int32_t minLen = m_numTiles;
    std::int32_t tmp = 0;

    for (std::int32_t i = 0; i < TABLE_SIZE; i++) {
        tmp = debugChainLen(i);
        if (tmp > maxLen)
            maxLen = tmp;
        if (tmp < minLen)
            minLen = tmp;
    }

    min = minLen;
    max = maxLen;
}

template<class T>
void KisTileHashTableTraits<T>::debugListLengthDistribution()
{
    std::int32_t min, max;
    std::int32_t arraySize;
    std::int32_t tmp;

    debugMaxListLength(min, max);
    arraySize = max - min + 1;

    std::int32_t *array = new std::int32_t[arraySize];
    memset(array, 0, sizeof(std::int32_t)*arraySize);

    for (std::int32_t i = 0; i < TABLE_SIZE; i++) {
        tmp = debugChainLen(i);
        array[tmp-min]++;
    }

    infoTiles << PkString("   minChain: %1\n").arg(min);
    infoTiles << PkString("   maxChain: %1\n").arg(max);

    infoTiles << "   Chain size distribution:";
    for (std::int32_t i = 0; i < arraySize; i++)
        infoTiles << PkString("      %1: %2").arg(i + min).arg(array[i]);

    delete[] array;
}

template<class T>
void KisTileHashTableTraits<T>::sanityChecksumCheck()
{
    /**
     * We assume that the lock should have already been taken
     * by the code that was going to change the table
     */
    assert(!m_lock.tryLockForWrite());

    TileTypeSP tile = 0;
    std::int32_t exactNumTiles = 0;

    for (std::int32_t i = 0; i < TABLE_SIZE; i++) {
        tile = m_hashTable[i];
        while (tile) {
            exactNumTiles++;
            tile = tile->next();
        }
    }

    if (exactNumTiles != m_numTiles) {
        dbgKrita << "Sanity check failed!";
        dbgKrita << ppVar(exactNumTiles);
        dbgKrita << ppVar(m_numTiles);
        dbgKrita << "Wrong tiles checksum!";
        assert(0); // not fatalKrita for a backtrace support
    }
}
