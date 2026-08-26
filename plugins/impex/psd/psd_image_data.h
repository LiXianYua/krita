/*
 *   SPDX-FileCopyrightText: 2011 Siddharth Sharma <siddharth.kde@gmail.com>
 *   SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PSD_IMAGE_DATA_H
#define PSD_IMAGE_DATA_H

#include <kis_paint_device.h>
#include <kis_types.h>

#include <psd.h>
#include <psd_header.h>
#include <compression.h>
#include <psd_layer_record.h>

#include <PkFileStream.h>
class PkStream;


class PSDImageData
{

public:
    PSDImageData(PSDHeader *header);
    virtual ~PSDImageData();

    bool read(PkStream &io, KisPaintDeviceSP dev);
    bool write(PkStream &io, KisPaintDeviceSP dev, bool hasAlpha, psd_compression_type compressionType);

    PkString error;

private:
    bool readRGB(PkStream &io, KisPaintDeviceSP dev);
    bool readCMYK(PkStream &io, KisPaintDeviceSP dev);
    bool readLAB(PkStream &io, KisPaintDeviceSP dev);
    bool readGrayscale(PkStream &io, KisPaintDeviceSP dev);

    PSDHeader *m_header {nullptr};

    quint16 m_compression {0};
    quint64 m_channelDataLength {0};
    quint32 m_channelSize {0};

    PkVector<ChannelInfo> m_channelInfoRecords;
    PkVector<int> m_channelOffsets; // this doesn't need to be global
};

#endif // PSD_IMAGE_DATA_H
