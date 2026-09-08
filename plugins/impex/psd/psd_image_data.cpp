/*
 *   SPDX-FileCopyrightText: 2011 Siddharth Sharma <siddharth.kde@gmail.com>
 *   SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <kis_debug.h>
#include <psd_image_data.h>

#include <cstdint>
#include <PkFileStream.h>
#include <PkVector.h>

#include <KoChannelInfo.h>
#include <KoColorSpace.h>
#include <kis_iterator_ng.h>
#include <kis_paint_device.h>

#include <asl/kis_asl_reader_utils.h>
#include <compression.h>
#include <psd_pixel_utils.h>
#include <psd_utils.h>

#undef qCDebug
#define qCDebug(category) PK_QCLOG_IMPL(category, isDebugEnabled, debug)

PSDImageData::PSDImageData(PSDHeader *header)
{
    m_header = header;
}

PSDImageData::~PSDImageData() {

}

bool PSDImageData::read(PkStream &io, KisPaintDeviceSP dev)
{
    psdread(io, m_compression);
    std::uint64_t start = io.pos();
    m_channelSize = m_header->channelDepth/8;
    m_channelDataLength = std::uint64_t(m_header->height) * m_header->width * m_channelSize;

    dbgFile << "Reading Image Data Block: compression" << m_compression << "channelsize" << m_channelSize << "number of channels" << m_header->nChannels;

    switch (m_compression) {

    case 0: // Uncompressed

        for (int channel = 0; channel < m_header->nChannels; channel++) {
            m_channelOffsets << 0;
            ChannelInfo channelInfo;
            channelInfo.channelId = channel;
            channelInfo.compressionType = psd_compression_type::Uncompressed;
            channelInfo.channelDataStart = start;
            channelInfo.channelDataLength = std::uint64_t(m_header->width) * m_header->height * m_channelSize;
            start += channelInfo.channelDataLength;
            m_channelInfoRecords.append(channelInfo);

        }

        break;

    case 1: // RLE
    {
        std::uint32_t rlelength = 0;

        // The start of the actual channel data is _after_ the RLE rowlengths block
        if (m_header->version == 1) {
            start += std::uint64_t(m_header->nChannels) * m_header->height * 2;
        }
        else if (m_header->version == 2) {
            start += std::uint64_t(m_header->nChannels) * m_header->height * 4;
        }

        for (int channel = 0; channel < m_header->nChannels; channel++) {
            m_channelOffsets << 0;
            std::uint64_t sumrlelength = 0;
            ChannelInfo channelInfo;
            channelInfo.channelId = channel;
            channelInfo.channelDataStart = start;
            channelInfo.compressionType = psd_compression_type::RLE;
            for (std::uint32_t row = 0; row < m_header->height; row++ ) {
                if (m_header->version == 1) {
                    std::uint16_t rlelength16 = 0; // use temporary variable to not cast pointers and not rely on endianness
                    psdread(io, rlelength16);
                    rlelength = rlelength16;
                }
                else if (m_header->version == 2) {
                    psdread(io, rlelength);
                }
                channelInfo.rleRowLengths.append(rlelength);
                sumrlelength += rlelength;
            }
            channelInfo.channelDataLength = sumrlelength;
            start += channelInfo.channelDataLength;
            m_channelInfoRecords.append(channelInfo);
        }

        break;
    }
    case 2: // ZIP without prediction
    case 3: // ZIP with prediction
    default:
        break;
    }

    if (!m_channelInfoRecords.isEmpty()) {
        PkVector<ChannelInfo*> infoRecords;

        PkVector<ChannelInfo>::iterator it = m_channelInfoRecords.begin();
        PkVector<ChannelInfo>::iterator end = m_channelInfoRecords.end();

        for (; it != end; ++it) {
            infoRecords << &(*it);
        }

        const PkRect imageRect(0, 0, m_header->width, m_header->height);

        try {
            PsdPixelUtils::readChannels(io, dev,
                                        m_header->colormode,
                                        m_channelSize,
                                        imageRect,
                                        infoRecords);
        } catch (KisAslReaderUtils::ASLParseException &) {
            dev->clear();
            return true;
        }
    }

    return true;
}

bool PSDImageData::write(PkStream &io, KisPaintDeviceSP dev, bool hasAlpha, psd_compression_type compressionType)
{
    psdwrite(io, static_cast<std::uint16_t>(compressionType));

    // now write all the channels in display order
    // fill in the channel chooser, in the display order, but store the pixel index as well.
    PkRect rc(0, 0, m_header->width, m_header->height);

    const int channelSize = m_header->channelDepth / 8;
    const psd_color_mode colorMode = m_header->colormode;

    PkVector<PsdPixelUtils::ChannelWritingInfo> writingInfoList;

    bool writeAlpha = hasAlpha &&
        dev->colorSpace()->channelCount() != dev->colorSpace()->colorChannelCount();

    const int numChannels =
        writeAlpha ?
        dev->colorSpace()->channelCount() :
        dev->colorSpace()->colorChannelCount();

    for (int i = 0; i < numChannels; i++) {
        const int rleOffset = io.pos();

        int channelId = writeAlpha && i == numChannels - 1 ? -1 : i;

        writingInfoList <<
            PsdPixelUtils::ChannelWritingInfo(channelId, -1, rleOffset);

        io.seek(io.pos() + rc.height() * sizeof(std::uint16_t));
    }

    PsdPixelUtils::writePixelDataCommon(io, dev, rc, colorMode, channelSize, false, false, writingInfoList, compressionType);
    return true;
}
