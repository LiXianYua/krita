/*
 *  SPDX-FileCopyrightText: 2009 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PSD_LAYER_RECORD_H
#define PSD_LAYER_RECORD_H

#include "kritapsd_export.h"

#include <PkByteArray.h>
#include <PkString.h>
#include <PkVector.h>

#include <cstdint>
#include <utility>

#include <kis_node.h>
#include <kis_paint_device.h>
#include <kis_types.h>
#include <psd.h>

#include "psd_additional_layer_info_block.h"
#include "psd_header.h"

class PkStream;
class KoPathShape;

enum psd_layer_type {
    psd_layer_type_normal,
    psd_layer_type_hidden,
    psd_layer_type_folder,
    psd_layer_type_solid_color,
    psd_layer_type_gradient_fill,
    psd_layer_type_pattern_fill,
    psd_layer_type_levels,
    psd_layer_type_curves,
    psd_layer_type_brightness_contrast,
    psd_layer_type_color_balance,
    psd_layer_type_hue_saturation,
    psd_layer_type_selective_color,
    psd_layer_type_threshold,
    psd_layer_type_invert,
    psd_layer_type_posterize,
    psd_layer_type_channel_mixer,
    psd_layer_type_gradient_map,
    psd_layer_type_photo_filter,
};

struct KRITAPSD_EXPORT ChannelInfo {
    ChannelInfo()
        : channelId(0)
        , compressionType(psd_compression_type::Unknown)
        , channelDataStart(0)
        , channelDataLength(0)
        , channelOffset(0)
        , channelInfoPosition(0)
    {
    }

    std::int16_t channelId; // 0 red, 1 green, 2 blue, -1 transparency, -2 user-supplied layer mask
    psd_compression_type compressionType;
    std::uint64_t channelDataStart;
    std::uint64_t channelDataLength;
    PkVector<std::uint32_t> rleRowLengths;
    int channelOffset; // where the channel data starts
    int channelInfoPosition; // where the channelinfo record is saved in the file
};

class KRITAPSD_EXPORT PSDLayerRecord
{
public:
    PSDLayerRecord(const PSDHeader &header);

    ~PSDLayerRecord()
    {
        qDeleteAll(channelInfoRecords);
    }

    PkRect channelRect(ChannelInfo *channel) const;

    bool read(PkStream &io);
    bool readPixelData(PkStream &io, KisPaintDeviceSP device);
    bool readMask(PkStream &io, KisPaintDeviceSP dev, ChannelInfo *channel);

    /**
     * @brief constructPathShape
     * create a KoPathshape based on a psd_path struct, used in vector masks and path resources.
     * @param path a psd path struct.
     * @param shapeWidth the image width in points
     * @param shapeHeight the image height in points
     * @param vogk extra vector data from the vogk layer info block.
     * @return a KoPathShape.
     */
    KoPathShape *constructPathShape(psd_path path, double shapeWidth, double shapeHeight);

    /**
     * @brief addPathShapeToPSDPath
     * add all KoPathShape subpaths to the given psd_path struct.
     * @param shapeWidth the image width in points
     * @param shapeHeight the image height in points
     */
    void addPathShapeToPSDPath(psd_path &path, KoPathShape *shape, double shapeWidth, double shapeHeight);

    void write(PkStream &io,
               KisPaintDeviceSP layerContentDevice,
               KisNodeSP onlyTransparencyMask,
               const PkRect &maskRect,
               psd_section_type sectionType,
               const PkXmlDocument &stylesXmlDoc,
               bool useLfxsLayerStyleFormat);
    void writePixelData(PkStream &io, psd_compression_type compressionType);

    bool valid();

    PkString error;

    std::int32_t top {0};
    std::int32_t left {0};
    std::int32_t bottom {0};
    std::int32_t right {0};

    std::uint16_t nChannels {0};

    PkVector<ChannelInfo *> channelInfoRecords;

    PkString blendModeKey;
    bool isPassThrough {false};

    std::uint8_t opacity {0};
    std::uint8_t clipping {0};
    bool transparencyProtected {false};
    bool visible {true};
    bool irrelevant {false};

    int labelColor {0};

    psd_fill_type fillType {psd_fill_solid_color};
    PkXmlDocument fillConfig;

    psd_vector_mask vectorMask;
    psd_layer_type_shape textShape;
    PkXmlDocument vectorStroke;
    PkXmlDocument vectorOriginationData;

    struct LayerMaskData {
        std::int32_t top {0};
        std::int32_t left {0};
        std::int32_t bottom {0};
        std::int32_t right {0};
        std::uint8_t defaultColor {255}; // 0 or 255
        bool positionedRelativeToLayer {false};
        bool disabled {false};
        bool invertLayerMaskWhenBlending {false};
        std::uint8_t userMaskDensity {0};
        double userMaskFeather {0.0};
        std::uint8_t vectorMaskDensity {0};
        double vectorMaskFeather {0.0};
    };

    LayerMaskData layerMask;

    struct LayerBlendingRanges {
        struct LayerBlendingRange {
            std::array<std::uint8_t, 2> blackValues;
            std::array<std::uint8_t, 2> whiteValues;
        };

        PkByteArray data;

        std::pair<LayerBlendingRange, LayerBlendingRange> compositeGrayRange;
        PkVector<std::pair<LayerBlendingRange, LayerBlendingRange>> sourceDestinationRanges;
    };

    LayerBlendingRanges blendingRanges;

    PkString layerName {"UNINITIALIZED"}; // pascal, not unicode!

    PsdAdditionalLayerInfoBlock infoBlocks;

private:
    template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
    bool readImpl(PkStream &io);

    template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
    void writeImpl(PkStream &io,
                   KisPaintDeviceSP layerContentDevice,
                   KisNodeSP onlyTransparencyMask,
                   const PkRect &maskRect,
                   psd_section_type sectionType,
                   const PkXmlDocument &stylesXmlDoc,
                   bool useLfxsLayerStyleFormat);

    template<psd_byte_order = psd_byte_order::psdBigEndian>
    void writeTransparencyMaskPixelData(PkStream &io);

    template<psd_byte_order = psd_byte_order::psdBigEndian>
    void writePixelDataImpl(PkStream &io, psd_compression_type compressionType);

    KisPaintDeviceSP convertMaskDeviceIfNeeded(KisPaintDeviceSP dev);

    std::uint16_t psdLabelColor(int colorLabelIndex);
    int kritaColorLabelIndex(std::uint16_t labelColor);

private:
    KisPaintDeviceSP m_layerContentDevice;
    KisNodeSP m_onlyTransparencyMask;
    PkRect m_onlyTransparencyMaskRect;
    std::int64_t m_transparencyMaskSizeOffset {0};

    const PSDHeader m_header;
};

KRITAPSD_EXPORT PkDebug operator<<(PkDebug dbg, const PSDLayerRecord &layer);
KRITAPSD_EXPORT PkDebug operator<<(PkDebug dbg, const ChannelInfo &layer);

inline PkDebug operator<<(PkDebug dbg, const PSDLayerRecord::LayerBlendingRanges::LayerBlendingRange &data)
{
    return dbg << data.blackValues[0] << data.blackValues[1] << data.whiteValues[0] << data.whiteValues[1];
}

#endif // PSD_LAYER_RECORD_H
