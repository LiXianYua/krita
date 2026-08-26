/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __PSD_PIXEL_UTILS_H
#define __PSD_PIXEL_UTILS_H

#include "kritapsd_export.h"

#include <PkRect.h>
#include <PkVector.h>
#include <psd.h>

#include "kis_types.h"

class PkStream;
struct ChannelInfo;
struct ChannelWritingInfo;

namespace PsdPixelUtils
{
struct KRITAPSD_EXPORT ChannelWritingInfo {
    ChannelWritingInfo()
        : channelId(0)
        , sizeFieldOffset(-1)
        , rleBlockOffset(-1)
    {
    }
    ChannelWritingInfo(qint16 _channelId, int _sizeFieldOffset)
        : channelId(_channelId)
        , sizeFieldOffset(_sizeFieldOffset)
        , rleBlockOffset(-1)
    {
    }
    ChannelWritingInfo(qint16 _channelId, int _sizeFieldOffset, int _rleBlockOffset)
        : channelId(_channelId)
        , sizeFieldOffset(_sizeFieldOffset)
        , rleBlockOffset(_rleBlockOffset)
    {
    }

    qint16 channelId;
    int sizeFieldOffset;
    int rleBlockOffset;
};

void KRITAPSD_EXPORT readChannels(PkStream &io,
                                  KisPaintDeviceSP device,
                                  psd_color_mode colorMode,
                                  int channelSize,
                                  const PkRect &layerRect,
                                  PkVector<ChannelInfo *> infoRecords,
                                  psd_byte_order byteOrder = psd_byte_order::psdBigEndian);

void KRITAPSD_EXPORT readAlphaMaskChannels(PkStream &io,
                                           KisPaintDeviceSP device,
                                           int channelSize,
                                           const PkRect &layerRect,
                                           PkVector<ChannelInfo *> infoRecords,
                                           psd_byte_order byteOrder = psd_byte_order::psdBigEndian);

void KRITAPSD_EXPORT writeChannelDataRLE(PkStream &io,
                                         const quint8 *plane,
                                         const int channelSize,
                                         const PkRect &rc,
                                         const qint64 sizeFieldOffset,
                                         const qint64 rleBlockOffset,
                                         const bool writeCompressionType,
                                         psd_byte_order byteOrder = psd_byte_order::psdBigEndian);

void KRITAPSD_EXPORT writePixelDataCommon(PkStream &io,
                                          KisPaintDeviceSP dev,
                                          const PkRect &rc,
                                          psd_color_mode colorMode,
                                          int channelSize,
                                          bool alphaFirst,
                                          const bool writeCompressionType,
                                          PkVector<ChannelWritingInfo> &writingInfoList,
                                          psd_compression_type compressionType,
                                          psd_byte_order byteOrder = psd_byte_order::psdBigEndian);
}

#endif /* __PSD_PIXEL_UTILS_H */
