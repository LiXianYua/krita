/*
 *  SPDX-FileCopyrightText: 2004 C. Boemann <cbo@boemann.dk>
 *  SPDX-FileCopyrightText: 2009 Dmitry Kazakov <dimula73@gmail.com>
 *  SPDX-FileCopyrightText: 2010 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <cstdint>
#include <cassert>

#include <PkRect.h>
#include <PkVector.h>
#include <PkPoint.h>
#include <PkStream.h>
#include <PkString.h>
#include <PkList.h>
#include <vector>
#include <string>

#include "kis_tile.h"
#include "kis_tiled_data_manager.h"
#include "kis_tile_data_wrapper.h"
#include "kis_tiled_data_manager_p.h"
#include "kis_memento_manager.h"
#include "swap/kis_legacy_tile_compressor.h"
#include "swap/kis_tile_compressor_factory.h"

#include "kis_paint_device_writer.h"

#include "kis_global.h"


/* The data area is divided into tiles each say 64x64 pixels (defined at compile time)
 * The tiles are laid out in a matrix that can have negative indexes.
 * The matrix grows automatically if needed (a call for writeaccess to a tile
 * outside the current extent)
 * Even though the matrix has grown it may still not contain tiles at specific positions.
 * They are created on demand
 */

KisTiledDataManager::KisTiledDataManager(std::uint32_t pixelSize,
                                         const std::uint8_t *defaultPixel)
{
    /* See comment in destructor for details */
    m_mementoManager = new KisMementoManager();
    m_hashTable = new KisTileHashTable(m_mementoManager);

    m_pixelSize = pixelSize;
    m_defaultPixel = new std::uint8_t[m_pixelSize];
    setDefaultPixel(defaultPixel);
}

KisTiledDataManager::KisTiledDataManager(const KisTiledDataManager &dm)
    : KisShared()
{
    /* See comment in destructor for details */

    /* We do not clone the history of the device, there is no usecase for it */
    m_mementoManager = new KisMementoManager();

    KisTileData *defaultTileData = dm.m_hashTable->refAndFetchDefaultTileData();
    m_mementoManager->setDefaultTileData(defaultTileData);
    defaultTileData->deref();

    m_hashTable = new KisTileHashTable(*dm.m_hashTable, m_mementoManager);

    m_pixelSize = dm.m_pixelSize;
    m_defaultPixel = new std::uint8_t[m_pixelSize];
    /**
     * We won't call setDefaultTileData here, as defaultTileDatas
     * has already been made shared in m_hashTable(dm->m_hashTable)
     */
    memcpy(m_defaultPixel, dm.m_defaultPixel, m_pixelSize);
    recalculateExtent();
}

KisTiledDataManager::~KisTiledDataManager()
{
    /**
     * Here is an  explanation why we use hash table  and The Memento Manager
     * dynamically allocated We need to  destroy them in that very order. The
     * reason is that when hash table destroying all her child tiles they all
     * cry about it  to The Memento Manager using a  pointer.  So The Memento
     * Manager should be alive during  that destruction. We could  use shared
     * pointers instead, but they create too much overhead.
     */
    delete m_hashTable;
    delete m_mementoManager;

    delete[] m_defaultPixel;
}

void KisTiledDataManager::setDefaultPixel(const std::uint8_t *defaultPixel)
{
    PkWriteLocker locker(&m_lock);
    setDefaultPixelImpl(defaultPixel);
}

void KisTiledDataManager::setDefaultPixelImpl(const std::uint8_t *defaultPixel)
{
    KisTileData *td = KisTileDataStore::instance()->createDefaultTileData(pixelSize(), defaultPixel);
    m_hashTable->setDefaultTileData(td);
    m_mementoManager->setDefaultTileData(td);

    memcpy(m_defaultPixel, defaultPixel, pixelSize());
}

bool KisTiledDataManager::write(KisPaintDeviceWriter &store)
{
    PkReadLocker locker(&m_lock);

    bool retval = true;

    if(CURRENT_VERSION == LEGACY_VERSION) {
        char str[80];
        snprintf(str, 80, "%d\n", m_hashTable->numTiles());
        retval = store.write(str, strlen(str));
    }
    else {
        retval = writeTilesHeader(store, m_hashTable->numTiles());
    }


    KisTileHashTableConstIterator iter(m_hashTable);
    KisTileSP tile;

    KisAbstractTileCompressorSP compressor =
        KisTileCompressorFactory::create(CURRENT_VERSION);

    while ((tile = iter.tile())) {
        retval = compressor->writeTile(tile, store);
        if (!retval) {
            warnFile << "Failed to write tile";
            break;
        }
        iter.next();
    }

    return retval;
}
bool KisTiledDataManager::read(PkStream *stream)
{
    clear();

    PkWriteLocker locker(&m_lock);
    KisMementoSP nothing = m_mementoManager->getMemento();

    if (!stream) {
        m_mementoManager->commit();
        return false;
    }

    const std::int32_t maxLineLength = 79; // Legacy magic
    char lineBuf[maxLineLength + 1];
    PkStream::pk_int64 lineLen = stream->readLine(lineBuf, maxLineLength + 1);
    PkString line = lineLen < 0 ? PkString() : PkString::PkFromUtf8(lineBuf, (int)lineLen);
    line = line.trimmed();

    std::uint32_t numTiles;
    std::int32_t tilesVersion = LEGACY_VERSION;

    if (!line.isEmpty() && line[0] == u'V') {
        std::vector<PkString> lineItems = line.split(u' ');

        PkString keyword = lineItems.front();
        assert(keyword == "VERSION");

        tilesVersion = lineItems.size() > 1 ? lineItems[1].toInt() : 0;

        if(!processTilesHeader(stream, numTiles))
            return false;
    }
    else {
        numTiles = (std::uint32_t)line.toInt();
    }

    KisAbstractTileCompressorSP compressor =
        KisTileCompressorFactory::create(tilesVersion);

    bool readSuccess = true;
    for (std::uint32_t i = 0; i < numTiles; i++) {
        if (!compressor->readTile(stream, this)) {
            readSuccess = false;
        }
    }

    m_mementoManager->commit();
    return readSuccess;
}

bool KisTiledDataManager::writeTilesHeader(KisPaintDeviceWriter &store, std::uint32_t numTiles)
{
    PkString buffer = PkString("VERSION %1\n"
                               "TILEWIDTH %2\n"
                               "TILEHEIGHT %3\n"
                               "PIXELSIZE %4\n"
                               "DATA %5\n")
        .arg(CURRENT_VERSION)
        .arg(KisTileData::WIDTH)
        .arg(KisTileData::HEIGHT)
        .arg((int)pixelSize())
        .arg((int)numTiles);

    std::string utf8 = buffer.PkToUtf8();
    return store.write(utf8.data(), (std::int64_t)utf8.size());
}

#define takeOneLine(stream, maxLine, keyword, value)            \
    do {                                                        \
        char lineBuf[maxLine + 1];                              \
        PkStream::pk_int64 lineLen = stream->readLine(lineBuf, maxLine + 1); \
        PkString line = lineLen < 0 ? PkString() : PkString::PkFromUtf8(lineBuf, (int)lineLen); \
        line = line.trimmed();                                  \
        std::vector<PkString> lineItems = line.split(u' ');     \
        keyword = lineItems.front();                            \
        value = lineItems.size() > 1 ? lineItems[1].toInt() : 0; \
    } while(0)                                                  \


bool KisTiledDataManager::processTilesHeader(PkStream *stream, std::uint32_t &numTiles)
{
    /**
     * We assume that there is only one version of this header
     * possible. In case we invent something new, it'll be quite easy
     * to modify the behavior
     */

    const std::int32_t maxLineLength = 25;
    const std::int32_t totalNumTests = 4;
    bool foundDataMark = false;
    std::int32_t testsPassed = 0;

    PkString keyword;
    std::int32_t value;

    while(!foundDataMark && stream->canReadLine()) {
        takeOneLine(stream, maxLineLength, keyword, value);

        if (keyword == "TILEWIDTH") {
            if(value != KisTileData::WIDTH)
                goto wrongString;
        }
        else if (keyword == "TILEHEIGHT") {
            if(value != KisTileData::HEIGHT)
                goto wrongString;
        }
        else if (keyword == "PIXELSIZE") {
            if((std::uint32_t)value != pixelSize())
                goto wrongString;
        }
        else if (keyword == "DATA") {
            numTiles = value;
            foundDataMark = true;
        }
        else {
            goto wrongString;
        }

        testsPassed++;
    }

    if(testsPassed != totalNumTests) {
        warnTiles << "Not enough fields of tiles header present"
                  << testsPassed << "of" << totalNumTests;
    }

    return testsPassed == totalNumTests;

wrongString:
    warnTiles << "Wrong string in tiles header:" << keyword << value;
    return false;
}

void KisTiledDataManager::purge(const PkRect& area)
{
    PkList<KisTileSP> tilesToDelete;
    {
        const std::int32_t tileDataSize = KisTileData::HEIGHT * KisTileData::WIDTH * pixelSize();
        KisTileData *tileData = m_hashTable->refAndFetchDefaultTileData();
        tileData->blockSwapping();
        const std::uint8_t *defaultData = tileData->data();

        KisTileHashTableConstIterator iter(m_hashTable);
        KisTileSP tile;

        while ((tile = iter.tile())) {
            if (tile->extent().intersects(area)) {
                tile->lockForRead();
                if(memcmp(defaultData, tile->data(), tileDataSize) == 0) {
                    tilesToDelete.append(tile);
                }
                tile->unlockForRead();
            }
            iter.next();
        }

        tileData->unblockSwapping();
        tileData->deref();
    }
    for (KisTileSP tile : tilesToDelete) {
        if (m_hashTable->deleteTile(tile)) {
            m_extentManager.notifyTileRemoved(tile->col(), tile->row());
        }
    }
}

std::uint8_t* KisTiledDataManager::duplicatePixel(std::int32_t num, const std::uint8_t *pixel)
{
    const std::int32_t pixelSize = this->pixelSize();
    /* FIXME:  Make a fun filling here */
    std::uint8_t *dstBuf = new std::uint8_t[num * pixelSize];
    std::uint8_t *dstIt = dstBuf;
    for (std::int32_t i = 0; i < num; i++) {
        memcpy(dstIt, pixel, pixelSize);
        dstIt += pixelSize;
    }
    return dstBuf;
}

void KisTiledDataManager::clear(PkRect clearRect, const std::uint8_t *clearPixel)
{
    if (clearPixel == 0)
        clearPixel = m_defaultPixel;

    if (clearRect.isEmpty())
        return;

    const std::int32_t pixelSize = this->pixelSize();

    bool pixelBytesAreDefault = !memcmp(clearPixel, m_defaultPixel, pixelSize);

    bool pixelBytesAreTheSame = true;
    for (std::int32_t i = 0; i < pixelSize; ++i) {
        if (clearPixel[i] != clearPixel[0]) {
            pixelBytesAreTheSame = false;
            break;
        }
    }

    if (pixelBytesAreDefault) {
        clearRect &= m_extentManager.extent();
    }

    std::int32_t firstColumn = xToCol(clearRect.left());
    std::int32_t lastColumn = xToCol(clearRect.right());

    std::int32_t firstRow = yToRow(clearRect.top());
    std::int32_t lastRow = yToRow(clearRect.bottom());

    const std::uint32_t rowStride = KisTileData::WIDTH * pixelSize;

    // Generate one row
    std::uint8_t *clearPixelData = 0;
    std::uint32_t maxRunLength = qMin(clearRect.width(), KisTileData::WIDTH);
    clearPixelData = duplicatePixel(maxRunLength, clearPixel);

    KisTileData *td = 0;
    if (!pixelBytesAreDefault &&
        clearRect.width() >= KisTileData::WIDTH &&
        clearRect.height() >= KisTileData::HEIGHT) {

        td = KisTileDataStore::instance()->createDefaultTileData(pixelSize, clearPixel);
        td->acquire();
    }

    for (std::int32_t row = firstRow; row <= lastRow; ++row) {
        for (std::int32_t column = firstColumn; column <= lastColumn; ++column) {

            PkRect tileRect(column*KisTileData::WIDTH, row*KisTileData::HEIGHT,
                           KisTileData::WIDTH, KisTileData::HEIGHT);
            PkRect clearTileRect = clearRect & tileRect;

            if (clearTileRect == tileRect) {
                 // Clear whole tile
                 const bool wasDeleted =
                     m_hashTable->deleteTile(column, row);

                 if (wasDeleted) {
                     m_extentManager.notifyTileRemoved(column, row);
                 }


                 if (!pixelBytesAreDefault) {
                     KisTileSP clearedTile = KisTileSP(new KisTile(column, row, td, m_mementoManager));
                     m_hashTable->addTile(clearedTile);
                     m_extentManager.notifyTileAdded(column, row);
                 }
            } else {
                const std::int32_t lineSize = clearTileRect.width() * pixelSize;
                std::int32_t rowsRemaining = clearTileRect.height();

                KisTileDataWrapper tw(this,
                                      clearTileRect.left(),
                                      clearTileRect.top(),
                                      KisTileDataWrapper::WRITE);
                std::uint8_t* tileIt = tw.data();

                if (pixelBytesAreTheSame) {
                    while (rowsRemaining > 0) {
                        memset(tileIt, *clearPixelData, lineSize);
                        tileIt += rowStride;
                        rowsRemaining--;
                    }
                } else {
                    while (rowsRemaining > 0) {
                        memcpy(tileIt, clearPixelData, lineSize);
                        tileIt += rowStride;
                        rowsRemaining--;
                    }
                }
            }
        }
    }

    if (td) td->release();
    delete[] clearPixelData;
}

void KisTiledDataManager::clear(PkRect clearRect, std::uint8_t clearValue)
{
    std::uint8_t *buf = new std::uint8_t[pixelSize()];
    memset(buf, clearValue, pixelSize());
    clear(clearRect, buf);
    delete[] buf;
}

void KisTiledDataManager::clear(std::int32_t x, std::int32_t y, std::int32_t w, std::int32_t h, const std::uint8_t *clearPixel)
{
    clear(PkRect(x, y, w, h), clearPixel);
}
void KisTiledDataManager::clear(std::int32_t x, std::int32_t y, std::int32_t w, std::int32_t h, std::uint8_t clearValue)
{
    clear(PkRect(x, y, w, h), clearValue);
}

void KisTiledDataManager::clear()
{
    m_hashTable->clear();
    m_extentManager.clear();
}


template<bool useOldSrcData>
void KisTiledDataManager::bitBltImpl(KisTiledDataManager *srcDM, const PkRect &rect)
{
    if (rect.isEmpty()) return;

    const std::int32_t pixelSize = this->pixelSize();
    const bool defaultPixelsCoincide =
        !memcmp(srcDM->defaultPixel(), m_defaultPixel, pixelSize);

    const std::uint32_t rowStride = KisTileData::WIDTH * pixelSize;

    std::int32_t firstColumn = xToCol(rect.left());
    std::int32_t lastColumn = xToCol(rect.right());

    std::int32_t firstRow = yToRow(rect.top());
    std::int32_t lastRow = yToRow(rect.bottom());

    for (std::int32_t row = firstRow; row <= lastRow; ++row) {
        for (std::int32_t column = firstColumn; column <= lastColumn; ++column) {

            bool srcTileExists = false;

            // this is the only variation in the template
            KisTileSP srcTile = useOldSrcData ?
                srcDM->getOldTile(column, row, srcTileExists) :
                srcDM->getReadOnlyTileLazy(column, row, srcTileExists);

            PkRect tileRect(column*KisTileData::WIDTH, row*KisTileData::HEIGHT,
                           KisTileData::WIDTH, KisTileData::HEIGHT);
            PkRect cloneTileRect = rect & tileRect;

            if (cloneTileRect == tileRect) {
                 // Clone whole tile
                 const bool wasDeleted =
                     m_hashTable->deleteTile(column, row);

                 if (srcTileExists || !defaultPixelsCoincide) {
                     srcTile->lockForRead();
                     KisTileData *td = srcTile->tileData();
                     KisTileSP clonedTile = KisTileSP(new KisTile(column, row, td, m_mementoManager));
                     srcTile->unlockForRead();

                     m_hashTable->addTile(clonedTile);

                     if (!wasDeleted) {
                         m_extentManager.notifyTileAdded(column, row);
                     }
                 } else if (wasDeleted) {
                     m_extentManager.notifyTileRemoved(column, row);
                 }

            } else {
                const std::int32_t lineSize = cloneTileRect.width() * pixelSize;
                std::int32_t rowsRemaining = cloneTileRect.height();

                KisTileDataWrapper tw(this,
                                      cloneTileRect.left(),
                                      cloneTileRect.top(),
                                      KisTileDataWrapper::WRITE);
                srcTile->lockForRead();
                // We suppose that the shift in both tiles is the same
                const std::uint8_t* srcTileIt = srcTile->data() + tw.offset();
                std::uint8_t* dstTileIt = tw.data();

                while (rowsRemaining > 0) {
                    memcpy(dstTileIt, srcTileIt, lineSize);
                    srcTileIt += rowStride;
                    dstTileIt += rowStride;
                    rowsRemaining--;
                }

                srcTile->unlockForRead();
            }
        }
    }
}

template<bool useOldSrcData>
void KisTiledDataManager::bitBltRoughImpl(KisTiledDataManager *srcDM, const PkRect &rect)
{
    if (rect.isEmpty()) return;

    const std::int32_t pixelSize = this->pixelSize();
    const bool defaultPixelsCoincide =
        !memcmp(srcDM->defaultPixel(), m_defaultPixel, pixelSize);

    std::int32_t firstColumn = xToCol(rect.left());
    std::int32_t lastColumn = xToCol(rect.right());

    std::int32_t firstRow = yToRow(rect.top());
    std::int32_t lastRow = yToRow(rect.bottom());

    for (std::int32_t row = firstRow; row <= lastRow; ++row) {
        for (std::int32_t column = firstColumn; column <= lastColumn; ++column) {

            /**
             * We are cloning whole tiles here so let's not be so boring
             * to check any borders :)
             */

            bool srcTileExists = false;

            // this is the only variation in the template
            KisTileSP srcTile = useOldSrcData ?
                srcDM->getOldTile(column, row, srcTileExists) :
                srcDM->getReadOnlyTileLazy(column, row, srcTileExists);

            const bool wasDeleted =
                m_hashTable->deleteTile(column, row);

            if (srcTileExists || !defaultPixelsCoincide) {
                srcTile->lockForRead();
                KisTileData *td = srcTile->tileData();
                KisTileSP clonedTile = KisTileSP(new KisTile(column, row, td, m_mementoManager));
                srcTile->unlockForRead();

                m_hashTable->addTile(clonedTile);

                if (!wasDeleted) {
                    m_extentManager.notifyTileAdded(column, row);
                }
            } else if (wasDeleted) {
                m_extentManager.notifyTileRemoved(column, row);
            }
        }
    }
}

void KisTiledDataManager::bitBlt(KisTiledDataManager *srcDM, const PkRect &rect)
{
    bitBltImpl<false>(srcDM, rect);
}

void KisTiledDataManager::bitBltOldData(KisTiledDataManager *srcDM, const PkRect &rect)
{
    bitBltImpl<true>(srcDM, rect);
}

void KisTiledDataManager::bitBltRough(KisTiledDataManager *srcDM, const PkRect &rect)
{
    bitBltRoughImpl<false>(srcDM, rect);
}

void KisTiledDataManager::bitBltRoughOldData(KisTiledDataManager *srcDM, const PkRect &rect)
{
    bitBltRoughImpl<true>(srcDM, rect);
}

void KisTiledDataManager::setExtent(std::int32_t x, std::int32_t y, std::int32_t w, std::int32_t h)
{
    setExtent(PkRect(x, y, w, h));
}

void KisTiledDataManager::setExtent(PkRect newRect)
{
    PkRect oldRect = extent();
    newRect = newRect.normalized();

    // Do nothing if the desired size is bigger than we currently are:
    // that is handled by the autoextending automatically
    if (newRect.contains(oldRect)) return;

    KisTileSP tile;
    PkRect tileRect;
    {
        KisTileHashTableIterator iter(m_hashTable);

        while (!iter.isDone()) {
            tile = iter.tile();

            tileRect = tile->extent();
            if (newRect.contains(tileRect)) {
                //do nothing
                iter.next();
            } else if (newRect.intersects(tileRect)) {
                PkRect intersection = newRect & tileRect;
                intersection.translate(- tileRect.topLeft());

                const std::int32_t pixelSize = this->pixelSize();

                tile->lockForWrite();
                std::uint8_t* data = tile->data();
                std::uint8_t* ptr;

                /* FIXME: make it faster */
                for (int y = 0; y < KisTileData::HEIGHT; y++) {
                    for (int x = 0; x < KisTileData::WIDTH; x++) {
                        if (!intersection.contains(x, y)) {
                            ptr = data + pixelSize * (y * KisTileData::WIDTH + x);
                            memcpy(ptr, m_defaultPixel, pixelSize);
                        }
                    }
                }
                tile->unlockForWrite();
                iter.next();
            } else {
                m_extentManager.notifyTileRemoved(tile->col(), tile->row());
                iter.deleteCurrent();
            }
        }
    }
}

void KisTiledDataManager::recalculateExtent()
{
    PkVector<PkPoint> indexes;

    {
        KisTileHashTableConstIterator iter(m_hashTable);
        KisTileSP tile;

        while ((tile = iter.tile())) {
            indexes.append(PkPoint(tile->col(), tile->row()));
            iter.next();
        }
    }

    m_extentManager.replaceTileStats(indexes);
}

void KisTiledDataManager::extent(std::int32_t &x, std::int32_t &y, std::int32_t &w, std::int32_t &h) const
{
    PkRect rect = extent();
    rect.getRect(&x, &y, &w, &h);
}

PkRect KisTiledDataManager::extent() const
{
    return m_extentManager.extent();
}

KisRegion KisTiledDataManager::region() const
{
    PkVector<PkRect> rects;

    KisTileHashTableConstIterator iter(m_hashTable);
    KisTileSP tile;

    while ((tile = iter.tile())) {
        rects.append(tile->extent());
        iter.next();
    }

    return KisRegion(std::move(rects));
}

void KisTiledDataManager::setPixel(std::int32_t x, std::int32_t y, const std::uint8_t * data)
{
    KisTileDataWrapper tw(this, x, y, KisTileDataWrapper::WRITE);
    memcpy(tw.data(), data, pixelSize());
}

void KisTiledDataManager::writeBytes(const std::uint8_t *data,
                                     std::int32_t x, std::int32_t y,
                                     std::int32_t width, std::int32_t height,
                                     std::int32_t dataRowStride)
{
    PkWriteLocker locker(&m_lock);
    // Actual bytes reading/writing is done in private header
    writeBytesBody(data, x, y, width, height, dataRowStride);
}

void KisTiledDataManager::readBytes(std::uint8_t *data,
                                    std::int32_t x, std::int32_t y,
                                    std::int32_t width, std::int32_t height,
                                    std::int32_t dataRowStride) const
{
    PkReadLocker locker(&m_lock);
    // Actual bytes reading/writing is done in private header
    readBytesBody(data, x, y, width, height, dataRowStride);
}

PkVector<std::uint8_t*>
KisTiledDataManager::readPlanarBytes(PkVector<std::int32_t> channelSizes,
                                     std::int32_t x, std::int32_t y,
                                     std::int32_t width, std::int32_t height) const
{
    PkReadLocker locker(&m_lock);
    // Actual bytes reading/writing is done in private header
    return readPlanarBytesBody(channelSizes, x, y, width, height);
}


void KisTiledDataManager::writePlanarBytes(PkVector<std::uint8_t*> planes,
                                           PkVector<std::int32_t> channelSizes,
                                           std::int32_t x, std::int32_t y,
                                           std::int32_t width, std::int32_t height)
{
    PkWriteLocker locker(&m_lock);
    // Actual bytes reading/writing is done in private header

    bool allChannelsPresent = true;

    for (const std::uint8_t* plane : planes) {
        if (!plane) {
            allChannelsPresent = false;
            break;
        }
    }

    if (allChannelsPresent) {
        writePlanarBytesBody<true>(planes, channelSizes, x, y, width, height);
    } else {
        writePlanarBytesBody<false>(planes, channelSizes, x, y, width, height);
    }
}

std::int32_t KisTiledDataManager::numContiguousColumns(std::int32_t x, std::int32_t minY, std::int32_t maxY) const
{
    std::int32_t numColumns;

    (void)(minY);
    (void)(maxY);

    if (x >= 0) {
        numColumns = KisTileData::WIDTH - (x % KisTileData::WIDTH);
    } else {
        numColumns = ((-x - 1) % KisTileData::WIDTH) + 1;
    }

    return numColumns;
}

std::int32_t KisTiledDataManager::numContiguousRows(std::int32_t y, std::int32_t minX, std::int32_t maxX) const
{
    std::int32_t numRows;

    (void)(minX);
    (void)(maxX);

    if (y >= 0) {
        numRows = KisTileData::HEIGHT - (y % KisTileData::HEIGHT);
    } else {
        numRows = ((-y - 1) % KisTileData::HEIGHT) + 1;
    }

    return numRows;
}

std::int32_t KisTiledDataManager::rowStride(std::int32_t x, std::int32_t y) const
{
    (void)(x);
    (void)(y);

    return KisTileData::WIDTH * pixelSize();
}

void KisTiledDataManager::releaseInternalPools()
{
    KisTileData::releaseInternalPools();
}
