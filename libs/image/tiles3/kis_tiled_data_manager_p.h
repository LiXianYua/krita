/*
 *  SPDX-FileCopyrightText: 2004 C. Boemann <cbo@boemann.dk>
 *  SPDX-FileCopyrightText: 2009 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */



#include <PkVector.h>
#include <cstdint>
#include <cassert>

/* FIXME: Think over SSE here */
void KisTiledDataManager::writeBytesBody(const std::uint8_t *data,
                                         std::int32_t x, std::int32_t y,
                                         std::int32_t width, std::int32_t height,
                                         std::int32_t dataRowStride)
{
    if (!data) return;

    width  = width < 0  ? 0 : width;
    height = height < 0 ? 0 : height;

    std::int32_t dataY = 0;
    std::int32_t imageY = y;
    std::int32_t rowsRemaining = height;
    const std::int32_t pixelSize = this->pixelSize();

    if (dataRowStride <= 0) {
        dataRowStride = pixelSize * width;
    }

    while (rowsRemaining > 0) {

        std::int32_t dataX = 0;
        std::int32_t imageX = x;
        std::int32_t columnsRemaining = width;
        std::int32_t numContiguousImageRows = numContiguousRows(imageY, imageX,
                                                          imageX + width - 1);

        std::int32_t rowsToWork = qMin(numContiguousImageRows, rowsRemaining);

        while (columnsRemaining > 0) {

            std::int32_t numContiguousImageColumns =
                    numContiguousColumns(imageX, imageY,
                                         imageY + rowsToWork - 1);

            std::int32_t columnsToWork = qMin(numContiguousImageColumns,
                                        columnsRemaining);

            KisTileDataWrapper tw(this, imageX, imageY, KisTileDataWrapper::WRITE);
            std::uint8_t *tileIt = tw.data();


            const std::int32_t tileRowStride = rowStride(imageX, imageY);

            const std::uint8_t *dataIt = data +
                    dataX * pixelSize + dataY * dataRowStride;

            const std::int32_t lineSize = columnsToWork * pixelSize;

            for (std::int32_t row = 0; row < rowsToWork; row++) {
                memcpy(tileIt, dataIt, lineSize);
                tileIt += tileRowStride;
                dataIt += dataRowStride;
            }

            imageX += columnsToWork;
            dataX += columnsToWork;
            columnsRemaining -= columnsToWork;
        }

        imageY += rowsToWork;
        dataY += rowsToWork;
        rowsRemaining -= rowsToWork;
    }
}


void KisTiledDataManager::readBytesBody(std::uint8_t *data,
                                        std::int32_t x, std::int32_t y,
                                        std::int32_t width, std::int32_t height,
                                        std::int32_t dataRowStride) const
{
    if (!data) return;

    width  = width < 0  ? 0 : width;
    height = height < 0 ? 0 : height;

    std::int32_t dataY = 0;
    std::int32_t imageY = y;
    std::int32_t rowsRemaining = height;
    const std::int32_t pixelSize = this->pixelSize();

    if (dataRowStride <= 0) {
        dataRowStride = pixelSize * width;
    }

    while (rowsRemaining > 0) {

        std::int32_t dataX = 0;
        std::int32_t imageX = x;
        std::int32_t columnsRemaining = width;
        std::int32_t numContiguousImageRows = numContiguousRows(imageY, imageX,
                                                          imageX + width - 1);

        std::int32_t rowsToWork = qMin(numContiguousImageRows, rowsRemaining);

        while (columnsRemaining > 0) {

            std::int32_t numContiguousImageColumns = numContiguousColumns(imageX, imageY,
                                                                    imageY + rowsToWork - 1);

            std::int32_t columnsToWork = qMin(numContiguousImageColumns,
                                        columnsRemaining);

            // XXX: Ugly const cast because of the old pixelPtr design copied from tiles1.
            KisTileDataWrapper tw(const_cast<KisTiledDataManager*>(this), imageX, imageY, KisTileDataWrapper::READ);
            std::uint8_t *tileIt = tw.data();


            const std::int32_t tileRowStride = rowStride(imageX, imageY);

            std::uint8_t *dataIt = data +
                    dataX * pixelSize + dataY * dataRowStride;

            const std::int32_t lineSize = columnsToWork * pixelSize;

            for (std::int32_t row = 0; row < rowsToWork; row++) {
                memcpy(dataIt, tileIt, lineSize);
                tileIt += tileRowStride;
                dataIt += dataRowStride;
            }

            imageX += columnsToWork;
            dataX += columnsToWork;
            columnsRemaining -= columnsToWork;
        }

        imageY += rowsToWork;
        dataY += rowsToWork;
        rowsRemaining -= rowsToWork;
    }
}


#define forEachChannel(_idx, _channelSize)                              \
    for(std::int32_t _idx=0, _channelSize=channelSizes[_idx];         \
    _idx<numChannels && (_channelSize=channelSizes[_idx], 1);   \
    _idx++)

template <bool allChannelsPresent>
void KisTiledDataManager::writePlanarBytesBody(PkVector </*const*/ std::uint8_t* > planes,
                                               PkVector<std::int32_t> channelSizes,
                                               std::int32_t x, std::int32_t y,
                                               std::int32_t width, std::int32_t height)
{
    PK_TILES_ASSERT(planes.size() == channelSizes.size());
    PK_TILES_ASSERT(planes.size() > 0);

    width  = width < 0  ? 0 : width;
    height = height < 0 ? 0 : height;

    const std::int32_t numChannels = planes.size();
    const std::int32_t pixelSize = this->pixelSize();

    std::int32_t dataY = 0;
    std::int32_t imageY = y;
    std::int32_t rowsRemaining = height;

    while (rowsRemaining > 0) {

        std::int32_t dataX = 0;
        std::int32_t imageX = x;
        std::int32_t columnsRemaining = width;
        std::int32_t numContiguousImageRows = numContiguousRows(imageY, imageX,
                                                          imageX + width - 1);

        std::int32_t rowsToWork = qMin(numContiguousImageRows, rowsRemaining);

        while (columnsRemaining > 0) {

            std::int32_t numContiguousImageColumns =
                    numContiguousColumns(imageX, imageY,
                                         imageY + rowsToWork - 1);
            std::int32_t columnsToWork = qMin(numContiguousImageColumns,
                                        columnsRemaining);

            const std::int32_t dataIdx = dataX + dataY * width;
            const std::int32_t tileRowStride = rowStride(imageX, imageY) -
                    columnsToWork * pixelSize;

            KisTileDataWrapper tw(this, imageX, imageY,
                                  KisTileDataWrapper::WRITE);
            std::uint8_t *tileItStart = tw.data();


            forEachChannel(i, channelSize) {
                if (allChannelsPresent || planes[i]) {
                    const std::uint8_t* planeIt = planes[i] + dataIdx * channelSize;
                    std::int32_t dataStride = (width - columnsToWork) * channelSize;
                    std::uint8_t* tileIt = tileItStart;

                    for (std::int32_t row = 0; row < rowsToWork; row++) {
                        for (int col = 0; col < columnsToWork; col++) {
                            memcpy(tileIt, planeIt, channelSize);
                            tileIt += pixelSize;
                            planeIt += channelSize;
                        }

                        tileIt += tileRowStride;
                        planeIt += dataStride;
                    }
                }

                tileItStart += channelSize;
            }

            imageX += columnsToWork;
            dataX += columnsToWork;
            columnsRemaining -= columnsToWork;
        }


        imageY += rowsToWork;
        dataY += rowsToWork;
        rowsRemaining -= rowsToWork;
    }
}

PkVector<std::uint8_t*> KisTiledDataManager::readPlanarBytesBody(PkVector<std::int32_t> channelSizes,
                                                          std::int32_t x, std::int32_t y,
                                                          std::int32_t width, std::int32_t height) const
{
    PK_TILES_ASSERT(channelSizes.size() > 0);

    width  = width < 0  ? 0 : width;
    height = height < 0 ? 0 : height;

    const std::int32_t numChannels = channelSizes.size();
    const std::int32_t pixelSize = this->pixelSize();

    PkVector<std::uint8_t*> planes;
    forEachChannel(i, channelSize) {
        planes.append(new std::uint8_t[width * height * channelSize]);
    }

    std::int32_t dataY = 0;
    std::int32_t imageY = y;
    std::int32_t rowsRemaining = height;

    while (rowsRemaining > 0) {

        std::int32_t dataX = 0;
        std::int32_t imageX = x;
        std::int32_t columnsRemaining = width;
        std::int32_t numContiguousImageRows = numContiguousRows(imageY, imageX,
                                                          imageX + width - 1);

        std::int32_t rowsToWork = qMin(numContiguousImageRows, rowsRemaining);

        while (columnsRemaining > 0) {

            std::int32_t numContiguousImageColumns =
                    numContiguousColumns(imageX, imageY,
                                         imageY + rowsToWork - 1);
            std::int32_t columnsToWork = qMin(numContiguousImageColumns,
                                        columnsRemaining);

            const std::int32_t dataIdx = dataX + dataY * width;
            const std::int32_t tileRowStride = rowStride(imageX, imageY) -
                    columnsToWork * pixelSize;

            // XXX: Ugly const cast because of the old pixelPtr design copied from tiles1.
            KisTileDataWrapper tw(const_cast<KisTiledDataManager*>(this), imageX, imageY,
                                  KisTileDataWrapper::READ);
            std::uint8_t *tileItStart = tw.data();


            forEachChannel(i, channelSize) {
                std::uint8_t* planeIt = planes[i] + dataIdx * channelSize;
                std::int32_t dataStride = (width - columnsToWork) * channelSize;
                std::uint8_t* tileIt = tileItStart;

                for (std::int32_t row = 0; row < rowsToWork; row++) {
                    for (int col = 0; col < columnsToWork; col++) {
                        memcpy(planeIt, tileIt, channelSize);
                        tileIt += pixelSize;
                        planeIt += channelSize;
                    }

                    tileIt += tileRowStride;
                    planeIt += dataStride;
                }
                tileItStart += channelSize;
            }

            imageX += columnsToWork;
            dataX += columnsToWork;
            columnsRemaining -= columnsToWork;
        }


        imageY += rowsToWork;
        dataY += rowsToWork;
        rowsRemaining -= rowsToWork;
    }
    return planes;
}

