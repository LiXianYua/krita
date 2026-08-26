/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "psd_pixel_utils.h"

#include <PkStream.h>
#include <PkMap.h>
#include <QtEndian>
#include <QtGlobal>

#include <KoColorSpace.h>
#include <KoColorSpaceMaths.h>
#include <KoColorSpaceTraits.h>
#include <colorspaces/KoAlphaColorSpace.h>
#include <kis_global.h>
#include <kis_iterator_ng.h>

#include <asl/kis_asl_reader_utils.h>
#include <asl/kis_asl_writer_utils.h>
#include <asl/kis_offset_keeper.h>
#include <compression.h>
#include <psd.h>
#include <psd_layer_record.h>

namespace PsdPixelUtils
{
template<class Traits>
typename Traits::channels_type convertByteOrder(typename Traits::channels_type value);
// default implementation is undefined for every color space should be added manually

template<>
inline quint8 convertByteOrder<AlphaU8Traits>(quint8 value)
{
    return value;
}

template<>
inline quint16 convertByteOrder<AlphaU16Traits>(quint16 value)
{
    return qFromBigEndian((quint16)value);
}

template<>
inline float convertByteOrder<AlphaF32Traits>(float value)
{
    return qFromBigEndian((quint32)value);
}

template<>
inline quint8 convertByteOrder<KoGrayU8Traits>(quint8 value)
{
    return value;
}

template<>
inline quint16 convertByteOrder<KoGrayU16Traits>(quint16 value)
{
    return qFromBigEndian((quint16)value);
}

template<>
inline quint32 convertByteOrder<KoGrayU32Traits>(quint32 value)
{
    return qFromBigEndian((quint32)value);
}

template<>
inline quint8 convertByteOrder<KoBgrU8Traits>(quint8 value)
{
    return value;
}

template<>
inline quint16 convertByteOrder<KoBgrU16Traits>(quint16 value)
{
    return qFromBigEndian((quint16)value);
}

template<>
inline quint32 convertByteOrder<KoBgrU32Traits>(quint32 value)
{
    return qFromBigEndian((quint32)value);
}

template<>
inline quint8 convertByteOrder<KoCmykU8Traits>(quint8 value)
{
    return value;
}

template<>
inline quint16 convertByteOrder<KoCmykU16Traits>(quint16 value)
{
    return qFromBigEndian((quint16)value);
}

template<>
inline float convertByteOrder<KoCmykF32Traits>(float value)
{
    return qFromBigEndian((quint32)value);
}

template<>
inline quint8 convertByteOrder<KoLabU8Traits>(quint8 value)
{
    return value;
}

template<>
inline quint16 convertByteOrder<KoLabU16Traits>(quint16 value)
{
    return qFromBigEndian((quint16)value);
}

template<>
inline float convertByteOrder<KoLabF32Traits>(float value)
{
    return qFromBigEndian((quint32)value);
}

template<class Traits>
inline quint8 truncateToOpacity(typename Traits::channels_type value);

template<>
inline quint8 truncateToOpacity<AlphaU8Traits>(typename AlphaU8Traits::channels_type value)
{
    return value;
}

template<>
inline quint8 truncateToOpacity<AlphaU16Traits>(typename AlphaU16Traits::channels_type value)
{
    return value >> 8;
}

template<>
inline quint8 truncateToOpacity<AlphaF32Traits>(typename AlphaF32Traits::channels_type value)
{
    return static_cast<quint8>(value * 255U);
}

template<class Traits, psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
void readAlphaMaskPixel(const PkMap<quint16, PkByteArray> &channelBytes, int col, quint8 *dstPtr)
{
    using channels_type = typename Traits::channels_type;

    const channels_type data = reinterpret_cast<const channels_type *>(channelBytes.first().constData())[col];
    if (byteOrder == psd_byte_order::psdBigEndian) {
        *dstPtr = truncateToOpacity<Traits>(convertByteOrder<Traits>(data));
    } else {
        *dstPtr = truncateToOpacity<Traits>(data);
    }
}

template<class Traits, psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
inline typename Traits::channels_type
readChannelValue(const PkMap<quint16, PkByteArray> &channelBytes, quint16 channelId, int col, typename Traits::channels_type defaultValue)
{
    using channels_type = typename Traits::channels_type;

    if (channelBytes.contains(channelId)) {
        const PkByteArray &bytes = channelBytes[channelId];
        if (col < bytes.size()) {
            const channels_type data = reinterpret_cast<const channels_type *>(bytes.constData())[col];
            if (byteOrder == psd_byte_order::psdBigEndian) {
                return convertByteOrder<Traits>(data);
            } else {
                return data;
            }
        }

        dbgFile << "col index out of range channelId: " << channelId << " col:" << col;
    }

    return defaultValue;
}

template<class Traits, psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
void readGrayPixel(const PkMap<quint16, PkByteArray> &channelBytes, int col, quint8 *dstPtr)
{
    using Pixel = typename Traits::Pixel;
    using channels_type = typename Traits::channels_type;

    const channels_type unitValue = KoColorSpaceMathsTraits<channels_type>::unitValue;
    Pixel *pixelPtr = reinterpret_cast<Pixel *>(dstPtr);

    pixelPtr->gray = readChannelValue<Traits, byteOrder>(channelBytes, 0, col, unitValue);
    pixelPtr->alpha = readChannelValue<Traits, byteOrder>(channelBytes, -1, col, unitValue);
}

template<class Traits, psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
void readRgbPixel(const PkMap<quint16, PkByteArray> &channelBytes, int col, quint8 *dstPtr)
{
    using Pixel = typename Traits::Pixel;
    using channels_type = typename Traits::channels_type;

    const channels_type unitValue = KoColorSpaceMathsTraits<channels_type>::unitValue;
    Pixel *pixelPtr = reinterpret_cast<Pixel *>(dstPtr);

    pixelPtr->blue = readChannelValue<Traits, byteOrder>(channelBytes, 2, col, unitValue);
    pixelPtr->green = readChannelValue<Traits, byteOrder>(channelBytes, 1, col, unitValue);
    pixelPtr->red = readChannelValue<Traits, byteOrder>(channelBytes, 0, col, unitValue);
    pixelPtr->alpha = readChannelValue<Traits, byteOrder>(channelBytes, -1, col, unitValue);
}

template<class Traits, psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
void readCmykPixel(const PkMap<quint16, PkByteArray> &channelBytes, int col, quint8 *dstPtr)
{
    using Pixel = typename Traits::Pixel;
    using channels_type = typename Traits::channels_type;

    const channels_type unitValue = KoColorSpaceMathsTraits<channels_type>::unitValue;
    Pixel *pixelPtr = reinterpret_cast<Pixel *>(dstPtr);

    pixelPtr->cyan = unitValue - readChannelValue<Traits, byteOrder>(channelBytes, 0, col, unitValue);
    pixelPtr->magenta = unitValue - readChannelValue<Traits, byteOrder>(channelBytes, 1, col, unitValue);
    pixelPtr->yellow = unitValue - readChannelValue<Traits, byteOrder>(channelBytes, 2, col, unitValue);
    pixelPtr->black = unitValue - readChannelValue<Traits, byteOrder>(channelBytes, 3, col, unitValue);
    pixelPtr->alpha = readChannelValue<Traits, byteOrder>(channelBytes, -1, col, unitValue);
}

template<class Traits, psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
void readLabPixel(const PkMap<quint16, PkByteArray> &channelBytes, int col, quint8 *dstPtr)
{
    using Pixel = typename Traits::Pixel;
    using channels_type = typename Traits::channels_type;

    const channels_type unitValue = KoColorSpaceMathsTraits<channels_type>::unitValue;
    Pixel *pixelPtr = reinterpret_cast<Pixel *>(dstPtr);

    pixelPtr->L = readChannelValue<Traits, byteOrder>(channelBytes, 0, col, unitValue);
    pixelPtr->a = readChannelValue<Traits, byteOrder>(channelBytes, 1, col, unitValue);
    pixelPtr->b = readChannelValue<Traits, byteOrder>(channelBytes, 2, col, unitValue);
    pixelPtr->alpha = readChannelValue<Traits, byteOrder>(channelBytes, -1, col, unitValue);
}

template<psd_byte_order byteOrder>
void readRgbPixelCommon(int channelSize, const PkMap<quint16, PkByteArray> &channelBytes, int col, quint8 *dstPtr)
{
    if (channelSize == 1) {
        readRgbPixel<KoBgrU8Traits, byteOrder>(channelBytes, col, dstPtr);
    } else if (channelSize == 2) {
        readRgbPixel<KoBgrU16Traits, byteOrder>(channelBytes, col, dstPtr);
    } else if (channelSize == 4) {
        readRgbPixel<KoBgrU16Traits, byteOrder>(channelBytes, col, dstPtr);
    }
}

template<psd_byte_order byteOrder>
void readGrayPixelCommon(int channelSize, const PkMap<quint16, PkByteArray> &channelBytes, int col, quint8 *dstPtr)
{
    if (channelSize == 1) {
        readGrayPixel<KoGrayU8Traits, byteOrder>(channelBytes, col, dstPtr);
    } else if (channelSize == 2) {
        readGrayPixel<KoGrayU16Traits, byteOrder>(channelBytes, col, dstPtr);
    } else if (channelSize == 4) {
        readGrayPixel<KoGrayU32Traits, byteOrder>(channelBytes, col, dstPtr);
    }
}

template<psd_byte_order byteOrder>
void readCmykPixelCommon(int channelSize, const PkMap<quint16, PkByteArray> &channelBytes, int col, quint8 *dstPtr)
{
    if (channelSize == 1) {
        readCmykPixel<KoCmykU8Traits, byteOrder>(channelBytes, col, dstPtr);
    } else if (channelSize == 2) {
        readCmykPixel<KoCmykU16Traits, byteOrder>(channelBytes, col, dstPtr);
    } else if (channelSize == 4) {
        readCmykPixel<KoCmykF32Traits, byteOrder>(channelBytes, col, dstPtr);
    }
}

template<psd_byte_order byteOrder>
void readLabPixelCommon(int channelSize, const PkMap<quint16, PkByteArray> &channelBytes, int col, quint8 *dstPtr)
{
    if (channelSize == 1) {
        readLabPixel<KoLabU8Traits, byteOrder>(channelBytes, col, dstPtr);
    } else if (channelSize == 2) {
        readLabPixel<KoLabU16Traits, byteOrder>(channelBytes, col, dstPtr);
    } else if (channelSize == 4) {
        readLabPixel<KoLabF32Traits, byteOrder>(channelBytes, col, dstPtr);
    }
}

template<psd_byte_order byteOrder>
void readAlphaMaskPixelCommon(int channelSize, const PkMap<quint16, PkByteArray> &channelBytes, int col, quint8 *dstPtr)
{
    if (channelSize == 1) {
        readAlphaMaskPixel<AlphaU8Traits, byteOrder>(channelBytes, col, dstPtr);
    } else if (channelSize == 2) {
        readAlphaMaskPixel<AlphaU16Traits, byteOrder>(channelBytes, col, dstPtr);
    } else if (channelSize == 4) {
        readAlphaMaskPixel<AlphaF32Traits, byteOrder>(channelBytes, col, dstPtr);
    }
}

PkMap<quint16, PkByteArray> fetchChannelsBytes(PkStream &io, PkVector<ChannelInfo *> channelInfoRecords, int row, int width, int channelSize, bool processMasks)
{
    const int uncompressedLength = width * channelSize;

    PkMap<quint16, PkByteArray> channelBytes;

    Q_FOREACH (ChannelInfo *channelInfo, channelInfoRecords) {
        // user supplied masks are ignored here
        if (!processMasks && channelInfo->channelId < -1)
            continue;

        io.seek(channelInfo->channelDataStart + channelInfo->channelOffset);

        if (channelInfo->compressionType == psd_compression_type::Uncompressed) {
            channelBytes[channelInfo->channelId] = io.read(uncompressedLength);
            channelInfo->channelOffset += uncompressedLength;
        } else if (channelInfo->compressionType == psd_compression_type::RLE) {
            int rleLength = channelInfo->rleRowLengths[row];
            PkByteArray compressedBytes = io.read(rleLength);
            PkByteArray uncompressedBytes = Compression::uncompress(uncompressedLength, compressedBytes, channelInfo->compressionType);
            channelBytes.insert(channelInfo->channelId, uncompressedBytes);
            channelInfo->channelOffset += rleLength;
        } else {
            PkString error = PkString("Unsupported Compression mode: %1")
                                .arg(static_cast<std::uint16_t>(channelInfo->compressionType));
            dbgFile << "ERROR: fetchChannelsBytes:" << error;
            throw KisAslReaderUtils::ASLParseException(error);
        }
    }

    return channelBytes;
}

using PixelFunc = std::function<void(int, const PkMap<quint16, PkByteArray> &, int, quint8 *)>;

void readCommon(KisPaintDeviceSP dev,
                PkStream &io,
                const PkRect &layerRect,
                PkVector<ChannelInfo *> infoRecords,
                int channelSize,
                PixelFunc pixelFunc,
                bool processMasks)
{
    KisOffsetKeeper keeper(io);

    if (layerRect.isEmpty()) {
        dbgFile << "Empty layer!";
        return;
    }

    if (infoRecords.first()->compressionType == psd_compression_type::ZIP || infoRecords.first()->compressionType == psd_compression_type::ZIPWithPrediction) {
        const int numPixels = channelSize * layerRect.width() * layerRect.height();

        PkMap<quint16, PkByteArray> channelBytes;

        Q_FOREACH (ChannelInfo *info, infoRecords) {
            io.seek(info->channelDataStart);
            PkByteArray compressedBytes = io.read(info->channelDataLength);
            PkByteArray uncompressedBytes;

            uncompressedBytes = Compression::uncompress(numPixels, compressedBytes, infoRecords.first()->compressionType, layerRect.width(), channelSize * 8);

            if (uncompressedBytes.size() != numPixels) {
                PkString error = PkString("Failed to unzip channel data: id = %1, compression = %2")
                                    .arg(info->channelId)
                                    .arg(static_cast<std::uint16_t>(info->compressionType));
                dbgFile << "ERROR:" << error;
                dbgFile << "      " << ppVar(info->channelId);
                dbgFile << "      " << ppVar(info->channelDataStart);
                dbgFile << "      " << ppVar(info->channelDataLength);
                dbgFile << "      " << ppVar(info->compressionType);
                throw KisAslReaderUtils::ASLParseException(error);
            }

            channelBytes.insert(info->channelId, uncompressedBytes);
        }

        KisSequentialIterator it(dev, layerRect);
        int col = 0;
        while (it.nextPixel()) {
            pixelFunc(channelSize, channelBytes, col, it.rawData());
            col++;
        }

    } else {
        KisHLineIteratorSP it = dev->createHLineIteratorNG(layerRect.left(), layerRect.top(), layerRect.width());
        for (int i = 0; i < layerRect.height(); i++) {
            PkMap<quint16, PkByteArray> channelBytes;

            channelBytes = fetchChannelsBytes(io, infoRecords, i, layerRect.width(), channelSize, processMasks);

            for (int col = 0; col < layerRect.width(); col++) {
                pixelFunc(channelSize, channelBytes, col, it->rawData());
                it->nextPixel();
            }

            /// don't write-access the row right after the
            /// the end of the read area
            if (i < layerRect.height() - 1) {
                it->nextRow();
            }
        }
    }
}

template<psd_byte_order byteOrder>
void readChannelsImpl(PkStream &io,
                      KisPaintDeviceSP device,
                      psd_color_mode colorMode,
                      int channelSize,
                      const PkRect &layerRect,
                      PkVector<ChannelInfo *> infoRecords)
{
    switch (colorMode) {
    case Grayscale:
        readCommon(device, io, layerRect, infoRecords, channelSize, &readGrayPixelCommon<byteOrder>, false);
        break;
    case RGB:
        readCommon(device, io, layerRect, infoRecords, channelSize, &readRgbPixelCommon<byteOrder>, false);
        break;
    case CMYK:
        readCommon(device, io, layerRect, infoRecords, channelSize, &readCmykPixelCommon<byteOrder>, false);
        break;
    case Lab:
        readCommon(device, io, layerRect, infoRecords, channelSize, &readLabPixelCommon<byteOrder>, false);
        break;
    case Bitmap:
    case Indexed:
    case MultiChannel:
    case DuoTone:
    case COLORMODE_UNKNOWN:
    default:
        PkString error = PkString("Unsupported color mode: %1").arg(colorMode);
        throw KisAslReaderUtils::ASLParseException(error);
    }
}

void readChannels(PkStream &io,
                  KisPaintDeviceSP device,
                  psd_color_mode colorMode,
                  int channelSize,
                  const PkRect &layerRect,
                  PkVector<ChannelInfo *> infoRecords,
                  psd_byte_order byteOrder)
{
    switch (byteOrder) {
    case psd_byte_order::psdLittleEndian:
        return readChannelsImpl<psd_byte_order::psdLittleEndian>(io, device, colorMode, channelSize, layerRect, infoRecords);
    default:
        return readChannelsImpl<psd_byte_order::psdBigEndian>(io, device, colorMode, channelSize, layerRect, infoRecords);
    }
}

template<psd_byte_order byteOrder>
void readAlphaMaskChannelsImpl(PkStream &io, KisPaintDeviceSP device, int channelSize, const PkRect &layerRect, PkVector<ChannelInfo *> infoRecords)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(infoRecords.size() == 1);
    readCommon(device, io, layerRect, infoRecords, channelSize, &readAlphaMaskPixelCommon<byteOrder>, true);
}

void readAlphaMaskChannels(PkStream &io,
                           KisPaintDeviceSP device,
                           int channelSize,
                           const PkRect &layerRect,
                           PkVector<ChannelInfo *> infoRecords,
                           psd_byte_order byteOrder)
{
    switch (byteOrder) {
    case psd_byte_order::psdLittleEndian:
        return readAlphaMaskChannelsImpl<psd_byte_order::psdLittleEndian>(io, device, channelSize, layerRect, infoRecords);
    default:
        return readAlphaMaskChannelsImpl<psd_byte_order::psdBigEndian>(io, device, channelSize, layerRect, infoRecords);
    }
}

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
void writeChannelDataRLEImpl(PkStream &io,
                             const quint8 *plane,
                             const int channelSize,
                             const PkRect &rc,
                             const qint64 sizeFieldOffset,
                             const qint64 rleBlockOffset,
                             const bool writeCompressionType)
{
    using Pusher = KisAslWriterUtils::OffsetStreamPusher<quint32, byteOrder>;
    PkScopedPointer<Pusher> channelBlockSizeExternalTag;
    if (sizeFieldOffset >= 0) {
        channelBlockSizeExternalTag.reset(new Pusher(io, 0, sizeFieldOffset));
    }

    if (writeCompressionType) {
        SAFE_WRITE_EX(byteOrder, io, static_cast<quint16>(psd_compression_type::RLE));
    }

    const bool externalRleBlock = rleBlockOffset >= 0;

    // the start of RLE sizes block
    const qint64 channelRLESizePos = externalRleBlock ? rleBlockOffset : io.pos();

    {
        PkScopedPointer<KisOffsetKeeper> rleOffsetKeeper;

        if (externalRleBlock) {
            rleOffsetKeeper.reset(new KisOffsetKeeper(io));
            io.seek(rleBlockOffset);
        }

        // write zero's for the channel lengths block
        for (int i = 0; i < rc.height(); ++i) {
            // XXX: choose size for PSB!
            const quint16 fakeRLEBLockSize = 0;
            SAFE_WRITE_EX(byteOrder, io, fakeRLEBLockSize);
        }
    }

    const int stride = channelSize * rc.width();
    for (qint32 row = 0; row < rc.height(); ++row) {
        PkByteArray uncompressed = PkByteArray::fromRawData((const char *)plane + row * stride, stride);
        PkByteArray compressed = Compression::compress(uncompressed, psd_compression_type::RLE);

        KisAslWriterUtils::OffsetStreamPusher<quint16, byteOrder> rleExternalTag(io, 0, channelRLESizePos + row * static_cast<qint64>(sizeof(quint16)));

        if (io.write(compressed) != compressed.size()) {
            throw KisAslWriterUtils::ASLWriteException("Failed to write image data");
        }
    }
}

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
void writeChannelDataZIPImpl(PkStream &io,
                             const quint8 *plane,
                             const int channelSize,
                             const PkRect &rc,
                             const qint64 sizeFieldOffset,
                             const bool writeCompressionType)
{
    using Pusher = KisAslWriterUtils::OffsetStreamPusher<quint32, byteOrder>;
    PkScopedPointer<Pusher> channelBlockSizeExternalTag;
    if (sizeFieldOffset >= 0) {
        channelBlockSizeExternalTag.reset(new Pusher(io, 0, sizeFieldOffset));
    }

    if (writeCompressionType) {
        SAFE_WRITE_EX(byteOrder, io, static_cast<quint16>(psd_compression_type::ZIP));
    }

    PkByteArray uncompressed(reinterpret_cast<const char *>(plane), rc.width() * rc.height() * channelSize);
    PkByteArray compressed(Compression::compress(uncompressed, psd_compression_type::ZIP));

    if (compressed.size() == 0 || io.write(compressed) != compressed.size()) {
        throw KisAslWriterUtils::ASLWriteException("Failed to write image data");
    }
}

void writeChannelDataRLE(PkStream &io,
                         const quint8 *plane,
                         const int channelSize,
                         const PkRect &rc,
                         const qint64 sizeFieldOffset,
                         const qint64 rleBlockOffset,
                         const bool writeCompressionType,
                         psd_byte_order byteOrder)
{
    switch (byteOrder) {
    case psd_byte_order::psdLittleEndian:
        return writeChannelDataRLEImpl<psd_byte_order::psdLittleEndian>(io, plane, channelSize, rc, sizeFieldOffset, rleBlockOffset, writeCompressionType);
    default:
        return writeChannelDataRLEImpl(io, plane, channelSize, rc, sizeFieldOffset, rleBlockOffset, writeCompressionType);
    }
}

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
inline void preparePixelForWrite(quint8 *dataPlane, int numPixels, int channelSize, int channelId, psd_color_mode colorMode)
{
    // if the bitdepth > 8, place the bytes in the right order
    // if cmyk, invert the pixel value
    if (channelSize == 1) {
        if (channelId >= 0 && (colorMode == CMYK || colorMode == CMYK64)) {
            for (int i = 0; i < numPixels; ++i) {
                dataPlane[i] = 255 - dataPlane[i];
            }
        }
    } else if (channelSize == 2) {
        quint16 val;
        for (int i = 0; i < numPixels; ++i) {
            quint16 *pixelPtr = reinterpret_cast<quint16 *>(dataPlane) + i;

            val = *pixelPtr;
            if (byteOrder == psd_byte_order::psdBigEndian)
                val = qFromBigEndian(val);
            if (channelId >= 0 && (colorMode == CMYK || colorMode == CMYK64)) {
                val = quint16_MAX - val;
            }
            *pixelPtr = val;
        }
    } else if (channelSize == 4) {
        quint32 val;
        for (int i = 0; i < numPixels; ++i) {
            quint32 *pixelPtr = reinterpret_cast<quint32 *>(dataPlane) + i;

            val = *pixelPtr;
            if (byteOrder == psd_byte_order::psdBigEndian)
                val = qFromBigEndian(val);
            if (channelId >= 0 && (colorMode == CMYK || colorMode == CMYK64)) {
                val = std::numeric_limits<quint32>::max() - val;
            }
            *pixelPtr = val;
        }
    }
}

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
void writePixelDataCommonImpl(PkStream &io,
                              KisPaintDeviceSP dev,
                              const PkRect &rc,
                              psd_color_mode colorMode,
                              int channelSize,
                              bool alphaFirst,
                              const bool writeCompressionType,
                              PkVector<ChannelWritingInfo> &writingInfoList,
                              psd_compression_type compressionType)
{
    // Empty rects must be processed separately on a higher level!
    KIS_ASSERT_RECOVER_RETURN(!rc.isEmpty());

    PkVector<quint8 *> tmp = dev->readPlanarBytes(rc.x() - dev->x(), rc.y() - dev->y(), rc.width(), rc.height());
    const KoColorSpace *colorSpace = dev->colorSpace();

    PkVector<quint8 *> planes;

    { // prepare 'planes' array

        quint8 *alphaPlanePtr = 0;

        const PkList<KoChannelInfo *> origChannels = colorSpace->channels();
        Q_FOREACH (KoChannelInfo *ch, KoChannelInfo::displayOrderSorted(origChannels)) {
            int channelIndex = KoChannelInfo::displayPositionToChannelIndex(ch->displayPosition(), origChannels);

            quint8 *holder = 0;
            std::swap(holder, tmp[channelIndex]);

            if (ch->channelType() == KoChannelInfo::ALPHA) {
                std::swap(holder, alphaPlanePtr);
            } else {
                planes.append(holder);
            }
        }

        if (alphaPlanePtr) {
            if (alphaFirst) {
                planes.insert(0, alphaPlanePtr);
                KIS_ASSERT_RECOVER_NOOP(writingInfoList.first().channelId == -1);
            } else {
                planes.append(alphaPlanePtr);
                KIS_ASSERT_RECOVER_NOOP((writingInfoList.size() == planes.size() - 1) || (writingInfoList.last().channelId == -1));
            }
        }

        // now planes are holding pointers to quint8 arrays
        tmp.clear();
    }

    KIS_ASSERT_RECOVER_RETURN(planes.size() >= writingInfoList.size());

    const int numPixels = rc.width() * rc.height();

    // write down the planes

    try {
        for (int i = 0; i < writingInfoList.size(); i++) {
            const ChannelWritingInfo &info = writingInfoList[i];

            dbgFile << "\tWriting channel" << i << "psd channel id" << info.channelId;

            // WARNING: Pixel data is ALWAYS in big endian!!!
            preparePixelForWrite<psd_byte_order::psdBigEndian>(planes[i], numPixels, channelSize, info.channelId, colorMode);

            dbgFile << "\t\tchannel start" << ppVar(io.pos()) << ", compression type" << compressionType;

            switch (compressionType) {
            case psd_compression_type::ZIP:
            case psd_compression_type::ZIPWithPrediction: {
                writeChannelDataZIPImpl<byteOrder>(io, planes[i], channelSize, rc, info.sizeFieldOffset, writeCompressionType);
                break;
            }
            case psd_compression_type::RLE:
            default: {
                writeChannelDataRLEImpl<byteOrder>(io, planes[i], channelSize, rc, info.sizeFieldOffset, info.rleBlockOffset, writeCompressionType);
                break;
            }
            }
        }

    } catch (KisAslWriterUtils::ASLWriteException &e) {
        Q_FOREACH (quint8 *plane, planes) {
            delete[] plane;
        }
        planes.clear();

        throw KisAslWriterUtils::ASLWriteException(PREPEND_METHOD(e.what()));
    }

    Q_FOREACH (quint8 *plane, planes) {
        delete[] plane;
    }
    planes.clear();
}

void writePixelDataCommon(PkStream &io,
                          KisPaintDeviceSP dev,
                          const PkRect &rc,
                          psd_color_mode colorMode,
                          int channelSize,
                          bool alphaFirst,
                          const bool writeCompressionType,
                          PkVector<ChannelWritingInfo> &writingInfoList,
                          psd_compression_type compressionType,
                          psd_byte_order byteOrder)
{
    switch (byteOrder) {
    case psd_byte_order::psdLittleEndian:
        return writePixelDataCommonImpl<psd_byte_order::psdLittleEndian>(io,
                                                                         dev,
                                                                         rc,
                                                                         colorMode,
                                                                         channelSize,
                                                                         alphaFirst,
                                                                         writeCompressionType,
                                                                         writingInfoList,
                                                                         compressionType);
    default:
        return writePixelDataCommonImpl(io, dev, rc, colorMode, channelSize, alphaFirst, writeCompressionType, writingInfoList, compressionType);
    }
}
}
