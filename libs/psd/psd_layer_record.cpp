/*
 *  SPDX-FileCopyrightText: 2009 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "psd_layer_record.h"

#include <KoColor.h>
#include <PkDataStream.h>
#include <PkStream.h>

#include "kis_iterator_ng.h"
#include <algorithm>
#include <kis_debug.h>
#include <kis_node.h>
#include <kis_paint_layer.h>

#include "compression.h"
#include "psd.h"
#include "psd_header.h"
#include "psd_utils.h"

#include <KoColorSpace.h>
#include <KoColorSpaceMaths.h>
#include <KoColorSpaceRegistry.h>
#include <KoColorSpaceTraits.h>

#include <KoPathShape.h>
#include <KoPathSegment.h>
#include <KoPathPoint.h>

#include <asl/kis_asl_reader_utils.h>
#include <asl/kis_asl_writer_utils.h>
#include <asl/kis_offset_keeper.h>

#include "psd_pixel_utils.h"
#include <kundo2command.h>

namespace {

// PkString::number() 尚未实现（R-01 已知缺口）；mask 参数的二进制调试输出用它。
PkString toBinaryString(unsigned int value)
{
    if (value == 0) {
        return PkString("0");
    }
    PkString out;
    while (value > 0) {
        out = ((value & 1) ? PkString("1") : PkString("0")) + out;
        value >>= 1;
    }
    return out;
}

} // namespace

// Just for pretty debug messages
PkString channelIdToChannelType(int channelId, psd_color_mode colormode)
{
    switch (channelId) {
    case -3:
        return "Real User Supplied Layer Mask (when both a user mask and a vector mask are present";
    case -2:
        return "User Supplied Layer Mask";
    case -1:
        return "Transparency mask";
    case 0:
        switch (colormode) {
        case Bitmap:
        case Indexed:
            return PkString("bitmap or indexed: %1").arg(channelId);
        case Grayscale:
        case Gray16:
            return "gray";
        case RGB:
        case RGB48:
            return "red";
        case Lab:
        case Lab48:
            return "L";
        case CMYK:
        case CMYK64:
            return "cyan";
        case MultiChannel:
        case DeepMultichannel:
            return PkString("multichannel channel %1").arg(channelId);
        case DuoTone:
        case Duotone16:
            return PkString("duotone channel %1").arg(channelId);
        default:
            return PkString("unknown: %1").arg(channelId);
        };
    case 1:
        switch (colormode) {
        case Bitmap:
        case Indexed:
            return PkString("WARNING bitmap or indexed: %1").arg(channelId);
        case Grayscale:
        case Gray16:
            return PkString("WARNING: %1").arg(channelId);
        case RGB:
        case RGB48:
            return "green";
        case Lab:
        case Lab48:
            return "a";
        case CMYK:
        case CMYK64:
            return "Magenta";
        case MultiChannel:
        case DeepMultichannel:
            return PkString("multichannel channel %1").arg(channelId);
        case DuoTone:
        case Duotone16:
            return PkString("duotone channel %1").arg(channelId);
        default:
            return PkString("unknown: %1").arg(channelId);
        };
    case 2:
        switch (colormode) {
        case Bitmap:
        case Indexed:
            return PkString("WARNING bitmap or indexed: %1").arg(channelId);
        case Grayscale:
        case Gray16:
            return PkString("WARNING: %1").arg(channelId);
        case RGB:
        case RGB48:
            return "blue";
        case Lab:
        case Lab48:
            return "b";
        case CMYK:
        case CMYK64:
            return "yellow";
        case MultiChannel:
        case DeepMultichannel:
            return PkString("multichannel channel %1").arg(channelId);
        case DuoTone:
        case Duotone16:
            return PkString("duotone channel %1").arg(channelId);
        default:
            return PkString("unknown: %1").arg(channelId);
        };
    case 3:
        switch (colormode) {
        case Bitmap:
        case Indexed:
            return PkString("WARNING bitmap or indexed: %1").arg(channelId);
        case Grayscale:
        case Gray16:
            return PkString("WARNING: %1").arg(channelId);
        case RGB:
        case RGB48:
            return PkString("alpha: %1").arg(channelId);
        case Lab:
        case Lab48:
            return PkString("alpha: %1").arg(channelId);
        case CMYK:
        case CMYK64:
            return "Key";
        case MultiChannel:
        case DeepMultichannel:
            return PkString("multichannel channel %1").arg(channelId);
        case DuoTone:
        case Duotone16:
            return PkString("duotone channel %1").arg(channelId);
        default:
            return PkString("unknown: %1").arg(channelId);
        };
    default:
        return PkString("unknown: %1").arg(channelId);
    };
}

PSDLayerRecord::PSDLayerRecord(const PSDHeader &header)
    : infoBlocks(header)
    , m_header(header)
{
}

bool PSDLayerRecord::read(PkStream &io)
{
    switch (m_header.byteOrder) {
    case psd_byte_order::psdLittleEndian:
        return readImpl<psd_byte_order::psdLittleEndian>(io);
    default:
        return readImpl(io);
    }
}

template<psd_byte_order byteOrder>
bool PSDLayerRecord::readImpl(PkStream &io)
{
    dbgFile << "Going to read layer record. Pos:" << io.pos();

    if (!psdread<byteOrder>(io, top) || !psdread<byteOrder>(io, left) || !psdread<byteOrder>(io, bottom) || !psdread<byteOrder>(io, right)
        || !psdread<byteOrder>(io, nChannels)) {
        error = "could not read layer record";
        return false;
    }

    dbgFile << "\ttop" << top << "left" << left << "bottom" << bottom << "right" << right << "number of channels" << nChannels;

    Q_ASSERT(top <= bottom);
    Q_ASSERT(left <= right);
    Q_ASSERT(nChannels > 0);

    if (nChannels < 1) {
        error = PkString("Not enough channels. Got: %1").arg(nChannels);
        return false;
    }

    if (nChannels > MAX_CHANNELS) {
        error = PkString("Too many channels. Got: %1").arg(nChannels);
        return false;
    }

    for (int i = 0; i < nChannels; ++i) {
        if (io.atEnd()) {
            error = "Could not read enough data for channels";
            return false;
        }

        ChannelInfo *info = new ChannelInfo;

        if (!psdread<byteOrder>(io, info->channelId)) {
            error = "could not read channel id";
            delete info;
            return false;
        }
        bool r;
        if (m_header.version == 1) {
            std::uint32_t channelDataLength;
            r = psdread<byteOrder>(io, channelDataLength);
            info->channelDataLength = (std::uint64_t)channelDataLength;
        } else {
            r = psdread<byteOrder>(io, info->channelDataLength);
        }
        if (!r) {
            error = "Could not read length for channel data";
            delete info;
            return false;
        }

        dbgFile << "\tchannel" << i << "id" << channelIdToChannelType(info->channelId, m_header.colormode) << "length" << info->channelDataLength << "start"
                << info->channelDataStart << "offset" << info->channelOffset << "channelInfoPosition" << info->channelInfoPosition;

        channelInfoRecords << info;
    }

    if (!psd_read_blendmode<byteOrder>(io, blendModeKey)) {
        error = PkString("Could not read blend mode key. Got: %1").arg(blendModeKey);
        return false;
    }

    dbgFile << "\tBlend mode" << blendModeKey << "pos" << io.pos();

    if (!psdread<byteOrder>(io, opacity)) {
        error = "Could not read opacity";
        return false;
    }

    dbgFile << "\tOpacity" << opacity << io.pos();

    if (!psdread<byteOrder>(io, clipping)) {
        error = "Could not read clipping";
        return false;
    }

    dbgFile << "\tclipping" << clipping << io.pos();

    std::uint8_t flags;
    if (!psdread<byteOrder>(io, flags)) {
        error = "Could not read flags";
        return false;
    }
    dbgFile << "\tflags" << flags << io.pos();

    transparencyProtected = flags & 1 ? true : false;

    dbgFile << "\ttransparency protected" << transparencyProtected;

    visible = flags & 2 ? false : true;

    dbgFile << "\tvisible" << visible;

    if (flags & 8) {
        irrelevant = flags & 16 ? true : false;
    } else {
        irrelevant = false;
    }

    dbgFile << "\tirrelevant" << irrelevant;

    dbgFile << "\tfiller at " << io.pos();

    std::uint8_t filler;
    if (!psdread<byteOrder>(io, filler) || filler != 0) {
        error = "Could not read padding";
        return false;
    }

    dbgFile << "\tGoing to read extra data length" << io.pos();

    std::uint32_t extraDataLength;
    if (!psdread<byteOrder>(io, extraDataLength) || io.bytesAvailable() < extraDataLength) {
        error = PkString("Could not read extra layer data: %1 at pos %2").arg(static_cast<int>(extraDataLength)).arg(static_cast<int>(io.pos()));
        return false;
    }

    dbgFile << "\tExtra data length" << extraDataLength;

    if (extraDataLength > 0) {
        dbgFile << "Going to read extra data field. Bytes available: " << io.bytesAvailable() << "pos" << io.pos();

        // See https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_22582
        std::uint32_t layerMaskLength = 1; // invalid...
        if (!psdread<byteOrder>(io, layerMaskLength) || io.bytesAvailable() < layerMaskLength) {
            error = PkString("Could not read layer mask length: %1").arg(static_cast<int>(layerMaskLength));
            return false;
        }

        layerMask = {};

        if (layerMaskLength == 0) {
            dbgFile << "\tNo layer mask/adjustment layer data. pos" << io.pos();
        } else {
            dbgFile << "\tReading layer mask/adjustment layer data. Length of block:" << layerMaskLength << "pos"
                    << io.pos();

            if (!psdread<byteOrder>(io, layerMask.top) || !psdread<byteOrder>(io, layerMask.left)
                || !psdread<byteOrder>(io, layerMask.bottom) || !psdread<byteOrder>(io, layerMask.right)
                || !psdread<byteOrder>(io, layerMask.defaultColor) || !psdread<byteOrder>(io, flags)) {
                error = "could not read common records of layer mask";
                return false;
            }

            layerMask.positionedRelativeToLayer = (flags & 1) != 0;
            layerMask.disabled = (flags & 2) != 0;
            layerMask.invertLayerMaskWhenBlending = (flags & 4) != 0;
            const bool hasMaskParameters = (flags & 8) != 0;

            dbgFile << "\tLayer mask info (original): position relative" << layerMask.positionedRelativeToLayer
                    << ", disabled" << layerMask.disabled << ", invert" << layerMask.invertLayerMaskWhenBlending
                    << ", needs to read mask parameters" << hasMaskParameters;

            if (layerMaskLength == 20) {
                std::uint16_t padding = 0;
                if (!psdread<byteOrder>(io, padding)) {
                    error = "Could not read layer mask padding";
                    return false;
                }
            } else {
                std::uint32_t remainingBlockLength = layerMaskLength - 18;

                dbgFile << "\tReading selective records from layer mask info. Remaining block length"
                        << remainingBlockLength;

                if (hasMaskParameters) {
                    if (!psdread<byteOrder>(io, flags)) {
                        error = "could not read mask parameters";
                        return false;
                    }

                    remainingBlockLength -= 1;

                    dbgFile << "\t\tMask parameters" << toBinaryString(flags) << ". Remaining block length"
                            << remainingBlockLength;

                    if (flags & 1) {
                        std::uint8_t dummy = 0;
                        if (!psdread<byteOrder>(io, dummy)) {
                            error = "could not read user mask density";
                            return false;
                        }
                        remainingBlockLength -= sizeof(dummy);
                    }

                    if (flags & 2) {
                        double dummy = 0;
                        if (!psdread<byteOrder>(io, dummy)) {
                            error = "could not read user mask feather";
                            return false;
                        }
                        remainingBlockLength -= sizeof(dummy);
                    }

                    if (flags & 4) {
                        std::uint8_t dummy = 0;
                        if (!psdread<byteOrder>(io, dummy)) {
                            error = "could not read vector mask density";
                            return false;
                        }
                        remainingBlockLength -= sizeof(dummy);
                    }

                    if (flags & 8) {
                        double dummy = 0;
                        if (!psdread<byteOrder>(io, dummy)) {
                            error = "could not read vector mask feather";
                            return false;
                        }
                        remainingBlockLength -= sizeof(dummy);
                    }
                }

                if (remainingBlockLength >= 1) {
                    if (!psdread<byteOrder>(io, flags)) {
                        error = "could not read 'real' mask record";
                        return false;
                    }

                    layerMask.positionedRelativeToLayer = (flags & 1) != 0;
                    layerMask.disabled = (flags & 2) != 0;
                    layerMask.invertLayerMaskWhenBlending = (flags & 4) != 0;
                    const bool hasMaskParameters = (flags & 8) != 0;

                    dbgFile << "\t\tLayer mask info (real): position relative" << layerMask.positionedRelativeToLayer
                            << ", disabled" << layerMask.disabled << ", invert" << layerMask.invertLayerMaskWhenBlending
                            << ", needs to read mask parameters" << hasMaskParameters;

                    remainingBlockLength -= 1;

                    dbgFile << "\t\tRemaining block length" << remainingBlockLength;
                }

                if (remainingBlockLength >= 1) {
                    if (!psdread<byteOrder>(io, layerMask.defaultColor)) {
                        error = "could not read 'real' default color";
                        return false;
                    }
                    remainingBlockLength -= 1;
                    dbgFile << "\t\tRead 'real' default color. Remaining block length" << remainingBlockLength;
                }

                if (remainingBlockLength >= 16) {
                    if (!psdread<byteOrder>(io, layerMask.top) || !psdread<byteOrder>(io, layerMask.left)
                        || !psdread<byteOrder>(io, layerMask.bottom) || !psdread<byteOrder>(io, layerMask.right)) {
                        error = "could not read 'real' mask rectangle";
                        return false;
                    }
                    remainingBlockLength -= 16;
                    dbgFile << "\t\tRead 'real' mask rectangle. Remaining block length" << remainingBlockLength;
                }
            }
        }

        // layer blending thingies
        std::uint32_t blendingDataLength = 0;
        if (!psdread<byteOrder>(io, blendingDataLength) || io.bytesAvailable() < blendingDataLength) {
            error = "Could not read extra blending data.";
            return false;
        }

        std::uint32_t blendingNchannels = blendingDataLength > 0 ? (blendingDataLength - 8) / 4 / 2 : 0;

        dbgFile << "\tNumber of blending channels:" << blendingNchannels;

        if (blendingDataLength > 0) {
            if (blendingDataLength > 0) {
                if (!psdread<byteOrder>(io, blendingRanges.compositeGrayRange.first.blackValues[0])
                    || !psdread<byteOrder>(io, blendingRanges.compositeGrayRange.first.blackValues[1])
                    || !psdread<byteOrder>(io, blendingRanges.compositeGrayRange.first.whiteValues[0])
                    || !psdread<byteOrder>(io, blendingRanges.compositeGrayRange.first.whiteValues[1])) {
                    error = "Could not read blending black/white values";
                    return false;
                }
            }
            blendingDataLength -= 4;

            if (blendingDataLength > 0) {
                if (!psdread<byteOrder>(io, blendingRanges.compositeGrayRange.second.blackValues[0])
                    || !psdread<byteOrder>(io, blendingRanges.compositeGrayRange.second.blackValues[1])
                    || !psdread<byteOrder>(io, blendingRanges.compositeGrayRange.second.whiteValues[0])
                    || !psdread<byteOrder>(io, blendingRanges.compositeGrayRange.second.whiteValues[1])) {
                    error = "Could not read blending black/white values";
                    return false;
                }
            }
            blendingDataLength -= 4;

            dbgFile << "\tBlending ranges:";
            dbgFile << "\t\tcomposite gray (source) :" << blendingRanges.compositeGrayRange.first;
            dbgFile << "\t\tcomposite gray (dest):" << blendingRanges.compositeGrayRange.second;

            for (std::uint32_t i = 0; i < blendingNchannels; ++i) {
                LayerBlendingRanges::LayerBlendingRange src{};
                LayerBlendingRanges::LayerBlendingRange dst{};
                if (!psdread<byteOrder>(io, src.blackValues[0]) || !psdread<byteOrder>(io, src.blackValues[1]) || !psdread<byteOrder>(io, src.whiteValues[0])
                    || !psdread<byteOrder>(io, src.whiteValues[1]) || !psdread<byteOrder>(io, dst.blackValues[0]) || !psdread<byteOrder>(io, dst.blackValues[1])
                    || !psdread<byteOrder>(io, dst.whiteValues[0]) || !psdread<byteOrder>(io, dst.whiteValues[1])) {
                    error = PkString("could not read src/dst range for channel %1").arg(static_cast<int>(i));
                    return false;
                }
                dbgFile << "\t\tread range " << src << "to" << dst << "for channel" << i;
                blendingRanges.sourceDestinationRanges << qMakePair(src, dst);
            }
        }

        dbgFile << "\tGoing to read layer name at" << io.pos();
        std::uint8_t layerNameLength;
        if (!psdread<byteOrder>(io, layerNameLength)) {
            error = "Could not read layer name length";
            return false;
        }

        dbgFile << "\tlayer name length unpadded" << layerNameLength << "pos" << io.pos();
        layerNameLength = ((layerNameLength + 1 + 3) & ~0x03) - 1;

        dbgFile << "\tlayer name length padded" << layerNameLength << "pos" << io.pos();
        // XXX: This should use psdread_pascalstring
        PkByteArray layerNameData;
        layerNameData.resize(layerNameLength + 1);
        const auto bytesRead = io.read(layerNameData.data(), layerNameLength);
        const int layerNameLen = bytesRead > 0 ? static_cast<int>(bytesRead) : 0;
        layerNameData.data()[layerNameLen] = '\0';
        layerName = PkString(layerNameData.constData());
        dbgFile << "\tlayer name" << layerName << io.pos();

        dbgFile << "\tAbout to read additional info blocks at" << io.pos();

        if (!infoBlocks.read(io)) {
            error = infoBlocks.error;
            return false;
        }

        if (infoBlocks.keys.contains("luni") && !infoBlocks.unicodeLayerName.isEmpty()) {
            layerName = infoBlocks.unicodeLayerName;
        }

        labelColor = kritaColorLabelIndex(infoBlocks.labelColor);
    }

    return valid();
}

void PSDLayerRecord::write(PkStream &io,
                           KisPaintDeviceSP layerContentDevice,
                           KisNodeSP onlyTransparencyMask,
                           const PkRect &maskRect,
                           psd_section_type sectionType,
                           const PkXmlDocument &stylesXmlDoc,
                           bool useLfxsLayerStyleFormat)
{
    switch (m_header.byteOrder) {
    case psd_byte_order::psdLittleEndian:
        return writeImpl<psd_byte_order::psdLittleEndian>(io,
                                                          layerContentDevice,
                                                          onlyTransparencyMask,
                                                          maskRect,
                                                          sectionType,
                                                          stylesXmlDoc,
                                                          useLfxsLayerStyleFormat);
    default:
        return writeImpl(io, layerContentDevice, onlyTransparencyMask, maskRect, sectionType, stylesXmlDoc, useLfxsLayerStyleFormat);
    }
}

template<psd_byte_order byteOrder>
void PSDLayerRecord::writeImpl(PkStream &io,
                               KisPaintDeviceSP layerContentDevice,
                               KisNodeSP onlyTransparencyMask,
                               const PkRect &maskRect,
                               psd_section_type sectionType,
                               const PkXmlDocument &stylesXmlDoc,
                               bool useLfxsLayerStyleFormat)
{
    dbgFile << "writing layer info record"
            << "at" << io.pos();

    m_layerContentDevice = layerContentDevice;
    m_onlyTransparencyMask = onlyTransparencyMask;
    m_onlyTransparencyMaskRect = maskRect;

    dbgFile << "saving layer record for " << layerName << "at pos" << io.pos();
    dbgFile << "\ttop" << top << "left" << left << "bottom" << bottom << "right" << right << "number of channels" << nChannels;
    Q_ASSERT(left <= right);
    Q_ASSERT(top <= bottom);
    Q_ASSERT(nChannels > 0);

    try {
        {
            const PkRect layerRect(left, top, right - left, bottom - top);
            KisAslWriterUtils::writeRect<byteOrder>(layerRect, io);
        }

        {
            std::uint16_t realNumberOfChannels = nChannels + bool(m_onlyTransparencyMask);
            SAFE_WRITE_EX(byteOrder, io, realNumberOfChannels);
        }

        for (ChannelInfo *channel : channelInfoRecords) {
            SAFE_WRITE_EX(byteOrder, io, (std::uint16_t)channel->channelId);

            channel->channelInfoPosition = static_cast<int>(io.pos());

            // to be filled in when we know how big channel block is
            const std::uint32_t fakeChannelSize = 0;
            SAFE_WRITE_EX(byteOrder, io, fakeChannelSize);
        }

        if (m_onlyTransparencyMask) {
            const std::uint16_t userSuppliedMaskChannelId = -2;
            SAFE_WRITE_EX(byteOrder, io, userSuppliedMaskChannelId);

            m_transparencyMaskSizeOffset = io.pos();

            const std::uint32_t fakeTransparencyMaskSize = 0;
            SAFE_WRITE_EX(byteOrder, io, fakeTransparencyMaskSize);
        }

        // blend mode
        dbgFile << ppVar(blendModeKey) << ppVar(io.pos());

        KisAslWriterUtils::writeFixedString<byteOrder>("8BIM", io);
        KisAslWriterUtils::writeFixedString<byteOrder>(blendModeKey, io);

        SAFE_WRITE_EX(byteOrder, io, opacity);
        SAFE_WRITE_EX(byteOrder, io, clipping); // unused

        // visibility and protection
        std::uint8_t flags = 0;
        if (transparencyProtected)
            flags |= 1;
        if (!visible)
            flags |= 2;
        flags |= (1 << 3);
        if (irrelevant) {
            flags |= (1 << 4);
        }

        SAFE_WRITE_EX(byteOrder, io, flags);

        {
            std::uint8_t padding = 0;
            SAFE_WRITE_EX(byteOrder, io, padding);
        }

        {
            // extra fields with their own length tag
            KisAslWriterUtils::OffsetStreamPusher<std::uint32_t, byteOrder> extraDataSizeTag(io);

            if (m_onlyTransparencyMask) {
                {
                    const std::uint32_t layerMaskDataSize = 20; // support simple case only
                    SAFE_WRITE_EX(byteOrder, io, layerMaskDataSize);
                }

                KisAslWriterUtils::writeRect<byteOrder>(m_onlyTransparencyMaskRect, io);

                {
                    // NOTE: in PSD the default color of the mask is stored in 1 byte value!
                    //       Even when the mask is actually 16/32 bit! I have no idea how it is
                    //       actually treated in this case.
                    KIS_ASSERT_RECOVER_NOOP(m_onlyTransparencyMask->paintDevice()->pixelSize() == 1);
                    const std::uint8_t defaultPixel = *m_onlyTransparencyMask->paintDevice()->defaultPixel().data();
                    SAFE_WRITE_EX(byteOrder, io, defaultPixel);
                }

                {
                    std::uint8_t maskFlags = 0; // nothing serious
                    if (!vectorMask.path.subPaths.isEmpty()) {
                        maskFlags |= 8; // bit 3 = indicates that the user mask actually came from rendering other data
                    }
                    SAFE_WRITE_EX(byteOrder, io, maskFlags);

                    const std::uint16_t padding = 0; // 2-byte padding
                    SAFE_WRITE_EX(byteOrder, io, padding);
                }
            } else {
                const std::uint32_t nullLayerMaskDataSize = 0;
                SAFE_WRITE_EX(byteOrder, io, nullLayerMaskDataSize);
            }

            {
                // blending ranges are not implemented yet
                const std::uint32_t nullBlendingRangesSize = 0;
                SAFE_WRITE_EX(byteOrder, io, nullBlendingRangesSize);
            }

            // layer name: Pascal string, padded to a multiple of 4 bytes.
            psdwrite_pascalstring<byteOrder>(io, layerName, 4);

            PsdAdditionalLayerInfoBlock additionalInfoBlock(m_header);

            // write 'luni' data block
            additionalInfoBlock.writeLuniBlockEx(io, layerName);

            additionalInfoBlock.writeLclrBlockEx(io, psdLabelColor(labelColor));

            // write 'lsct' data block
            if (sectionType != psd_other) {
                additionalInfoBlock.writeLsctBlockEx(io, sectionType, isPassThrough, blendModeKey);
            }

            // write 'lfx2' data block
            if (!stylesXmlDoc.isNull()) {
                additionalInfoBlock.writeLfx2BlockEx(io, stylesXmlDoc, useLfxsLayerStyleFormat);
            }

            // write SoCo, GdFl, PtFl data blocks.
            if (!fillConfig.isNull()) {
                additionalInfoBlock.writeFillLayerBlockEx(io, fillConfig, fillType);
            }

            // write 'vmsk' data block
            if (!vectorMask.path.subPaths.isEmpty()) {
                additionalInfoBlock.writeVmskBlockEx(io, vectorMask);
            }

            // write 'tysh' data block
            if (!textShape.engineData.empty()) {
                additionalInfoBlock.writeTypeToolBlockEx(io, textShape);
            }

            // write 'vstk' data block
            if (!vectorStroke.isNull()) {
                additionalInfoBlock.writeVectorStrokeDataEx(io, vectorStroke);
            }

            if (!vectorOriginationData.isNull()) {
                additionalInfoBlock.writeVectorOriginationDataEx(io, vectorOriginationData);
            }

        }
    } catch (KisAslWriterUtils::ASLWriteException &e) {
        throw KisAslWriterUtils::ASLWriteException(PREPEND_METHOD(e.what()));
    }
}

KisPaintDeviceSP PSDLayerRecord::convertMaskDeviceIfNeeded(KisPaintDeviceSP dev)
{
    KisPaintDeviceSP result = dev;

    if (m_header.channelDepth == 16) {
        result = new KisPaintDevice(*dev);
        result->convertTo(KoColorSpaceRegistry::instance()->alpha16());
    } else if (m_header.channelDepth == 32) {
        result = new KisPaintDevice(*dev);
        result->convertTo(KoColorSpaceRegistry::instance()->alpha32f());
    }
    return result;
}

std::uint16_t PSDLayerRecord::psdLabelColor(int colorLabelIndex)
{
    std::uint16_t color = 0;
    switch (colorLabelIndex) {
    case 0: // none
        color = 0;
        break;
    case 1: // Blue
        color = 5;
        break;
    case 2: // Green
        color = 4;
        break;
    case 3: // Yellow
        color = 3;
        break;
    case 4: // Orange
        color = 2;
        break;
    case 5: // Brown, don't save.
        color = 0;
        break;
    case 6: // Red
        color = 1;
        break;
    case 7: // Purple
        color = 6;
        break;
    case 8: // Grey
        color = 7;
        break;
    default:
        color = 0;
    }
    return color;
}

int PSDLayerRecord::kritaColorLabelIndex(std::uint16_t labelColor)
{
    int color = 0;
    switch (labelColor) {
    case 0:
        color = 0;
        break;
    case 1: // red
        color = 6;
        break;
    case 2: // Orange
        color = 4;
        break;
    case 3: // Yellow
        color = 3;
        break;
    case 4: // Green
        color = 2;
        break;
    case 5: // Blue
        color = 1;
        break;
    case 6: // Purple
        color = 7;
        break;
    case 7: // Grey
        color = 8;
        break;
    default:
        color = 0;
    }
    return color;
}

template<psd_byte_order byteOrder>
void PSDLayerRecord::writeTransparencyMaskPixelData(PkStream &io)
{
    if (m_onlyTransparencyMask) {
        KisPaintDeviceSP device = convertMaskDeviceIfNeeded(m_onlyTransparencyMask->paintDevice());

        PkByteArray buffer;
        buffer.resize(static_cast<int>(device->pixelSize()) * m_onlyTransparencyMaskRect.width() * m_onlyTransparencyMaskRect.height());
        device->readBytes((std::uint8_t *)buffer.data(), m_onlyTransparencyMaskRect);

        PsdPixelUtils::writeChannelDataRLE(io,
                                           (std::uint8_t *)buffer.data(),
                                           static_cast<int>(device->pixelSize()),
                                           m_onlyTransparencyMaskRect,
                                           m_transparencyMaskSizeOffset,
                                           -1,
                                           true,
                                           byteOrder);
    }
}

void PSDLayerRecord::writePixelData(PkStream &io, psd_compression_type compressionType)
{
    try {
        switch (m_header.byteOrder) {
        case psd_byte_order::psdLittleEndian:
            writePixelDataImpl<psd_byte_order::psdLittleEndian>(io, compressionType);
            break;
        default:
            writePixelDataImpl(io, compressionType);
            break;
        }
    } catch (KisAslWriterUtils::ASLWriteException &e) {
        throw KisAslWriterUtils::ASLWriteException(PREPEND_METHOD(e.what()));
    }
}

template<psd_byte_order byteOrder>
void PSDLayerRecord::writePixelDataImpl(PkStream &io, psd_compression_type compressionType)
{
    dbgFile << "writing pixel data for layer" << layerName << "at" << io.pos();

    KisPaintDeviceSP dev = m_layerContentDevice;
    const PkRect rc(left, top, right - left, bottom - top);

    if (rc.isEmpty()) {
        dbgFile << "Layer is empty! Writing placeholder information.";

        for (int i = 0; i < nChannels; i++) {
            const ChannelInfo *channelInfo = channelInfoRecords[i];
            KisAslWriterUtils::OffsetStreamPusher<std::uint32_t, byteOrder> channelBlockSizeExternalTag(io, 0, channelInfo->channelInfoPosition);
            SAFE_WRITE_EX(byteOrder, io, static_cast<std::uint16_t>(psd_compression_type::Uncompressed));
        }

        writeTransparencyMaskPixelData<byteOrder>(io);

        return;
    }

    // now write all the channels in display order
    dbgFile << "layer" << layerName;

    const int channelSize = m_header.channelDepth / 8;
    const psd_color_mode colorMode = m_header.colormode;

    PkVector<PsdPixelUtils::ChannelWritingInfo> writingInfoList;
    for (const ChannelInfo *channelInfo : channelInfoRecords) {
        writingInfoList << PsdPixelUtils::ChannelWritingInfo(channelInfo->channelId, channelInfo->channelInfoPosition);
    }

    PsdPixelUtils::writePixelDataCommon(io, dev, rc, colorMode, channelSize, true, true, writingInfoList, compressionType, byteOrder);
    writeTransparencyMaskPixelData<byteOrder>(io);
}

bool PSDLayerRecord::valid()
{
    // XXX: check validity!
    return true;
}

bool PSDLayerRecord::readPixelData(PkStream &io, KisPaintDeviceSP device)
{
    dbgFile << "Reading pixel data for layer" << layerName << "pos" << io.pos();

    const int channelSize = m_header.channelDepth / 8;
    const PkRect layerRect = PkRect(left, top, right - left, bottom - top);

    try {
        // WARNING: Pixel data is ALWAYS in big endian!!!
        PsdPixelUtils::readChannels(io, device, m_header.colormode, channelSize, layerRect, channelInfoRecords, psd_byte_order::psdBigEndian);
    } catch (KisAslReaderUtils::ASLParseException &e) {
        device->clear();
        error = e.what();
        return false;
    }

    return true;
}

PkRect PSDLayerRecord::channelRect(ChannelInfo *channel) const
{
    PkRect result;

    if (channel->channelId < -1) {
        result = PkRect(layerMask.left, layerMask.top, layerMask.right - layerMask.left, layerMask.bottom - layerMask.top);
    } else {
        result = PkRect(left, top, right - left, bottom - top);
    }

    return result;
}

bool PSDLayerRecord::readMask(PkStream &io, KisPaintDeviceSP dev, ChannelInfo *channelInfo)
{
    KIS_ASSERT_RECOVER(channelInfo->channelId < -1)
    {
        return false;
    }

    dbgFile << "Going to read" << channelIdToChannelType(channelInfo->channelId, m_header.colormode) << "mask";

    PkRect maskRect = channelRect(channelInfo);
    if (maskRect.isEmpty()) {
        dbgFile << "Empty Channel";
        return true;
    }

    // the device must be a pixel selection
    KIS_ASSERT_RECOVER(dev->pixelSize() == 1)
    {
        return false;
    }

    dev->setDefaultPixel(KoColor(&layerMask.defaultColor, dev->colorSpace()));

    const int pixelSize = m_header.channelDepth == 16 ? 2 : m_header.channelDepth == 32 ? 4 : 1;

    PkVector<ChannelInfo *> infoRecords;
    infoRecords << channelInfo;
    PsdPixelUtils::readAlphaMaskChannels(io, dev, pixelSize, maskRect, infoRecords);

    return true;
}

KoPathShape *PSDLayerRecord::constructPathShape(psd_path path, double shapeWidth, double shapeHeight)
{
    KoPathShape *shape = new KoPathShape();

    // psd paths are stored normalized.
    PkTransform tf = PkTransform::fromScale(shapeWidth, shapeHeight);

    PkString nodeTypes;
    for (psd_path_sub_path subPath : path.subPaths) {
        for (int i = 0; i < subPath.nodes.size(); i++) {
            psd_path_node node = subPath.nodes.at(i);
            if (i == 0) {
                shape->moveTo(tf.map(node.node));
            } else {
                psd_path_node previousNode = subPath.nodes.at(i-1);
                if (previousNode.node == previousNode.control2 && node.node == node.control1) {
                    shape->lineTo(tf.map(node.node));
                } else {
                    shape->curveTo(tf.map(previousNode.control2), tf.map(node.control1), tf.map(node.node));
                }
            }
            if (node.isSmooth) {
                nodeTypes.append("s");
            } else {
                nodeTypes.append("c");
            }
        }
        if (subPath.isClosed) {
            psd_path_node lastNode = subPath.nodes.last();
            psd_path_node firstNode = subPath.nodes.first();
            if (lastNode.node == lastNode.control2 && firstNode.node == firstNode.control1) {
                shape->lineTo(tf.map(firstNode.node));
            } else {
                shape->curveTo(tf.map(lastNode.control2), tf.map(firstNode.control1), tf.map(firstNode.node));
            }
            shape->closeMerge();
        }

    }
    if (shape->pointCount() > 0) {
        shape->loadNodeTypes(nodeTypes);
    }

    return shape;
}

void PSDLayerRecord::addPathShapeToPSDPath(psd_path &path, KoPathShape *shape, double shapeWidth, double shapeHeight)
{
    PkTransform tf = PkTransform::fromScale(shapeWidth, shapeHeight).inverted();
    tf = shape->absoluteTransformation()*tf;


    for (int i = 0; i < shape->subpathCount(); i++) {
        psd_path_sub_path subPath;
        subPath.isClosed = shape->isClosedSubpath(i);
        while(subPath.nodes.size() < shape->subpathPointCount(i)) {
            const KoPathPoint *point = shape->pointByIndex(KoPathPointIndex(i, subPath.nodes.size()));
            psd_path_node node;
            node.node = tf.map(point->point());
            node.control1 = point->activeControlPoint1()? tf.map(point->controlPoint1()): node.node;
            node.control2 = point->activeControlPoint2()? tf.map(point->controlPoint2()): node.node;

            node.isSmooth = (point->properties().testFlag(KoPathPoint::IsSmooth)
                    || point->properties().testFlag(KoPathPoint::IsSymmetric));
            subPath.nodes.append(node);
        }

        path.subPaths.append(subPath);
    }
}

PkDebug operator<<(PkDebug dbg, const PSDLayerRecord &layer)
{
#ifndef NODEBUG
    dbg.nospace() << "valid: " << const_cast<PSDLayerRecord *>(&layer)->valid();
    dbg.nospace() << ", name: " << layer.layerName;
    dbg.nospace() << ", top: " << layer.top;
    dbg.nospace() << ", left:" << layer.left;
    dbg.nospace() << ", bottom: " << layer.bottom;
    dbg.nospace() << ", right: " << layer.right;
    dbg.nospace() << ", number of channels: " << layer.nChannels;
    dbg.nospace() << ", blendModeKey: " << layer.blendModeKey;
    dbg.nospace() << ", opacity: " << layer.opacity;
    dbg.nospace() << ", clipping: " << layer.clipping;
    dbg.nospace() << ", transparency protected: " << layer.transparencyProtected;
    dbg.nospace() << ", visible: " << layer.visible;
    dbg.nospace() << ", irrelevant: " << layer.irrelevant << "\n";
    for (const ChannelInfo *channel : layer.channelInfoRecords) {
        dbg.space() << channel;
    }
#endif
    return dbg.nospace();
}

PkDebug operator<<(PkDebug dbg, const ChannelInfo &channel)
{
#ifndef NODEBUG
    dbg.nospace() << "\tChannel type" << channel.channelId << "size: " << channel.channelDataLength << "compression type" << channel.compressionType << "\n";
#endif
    return dbg.nospace();
}
