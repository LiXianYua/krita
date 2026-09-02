/*
 *  SPDX-FileCopyrightText: 2009 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <PkFlakeBridge.h>

#include "psd_layer_section.h"

#include <PkStream.h>
#include <PkByteArray.h>

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <vector>

#include <KoColor.h>
#include <KoColorSpace.h>

#include <kis_debug.h>
#include <kis_effect_mask.h>
#include <kis_group_layer.h>
#include <kis_generator_layer.h>
#include <kis_image.h>
#include <kis_node.h>
#include <kis_paint_layer.h>
#include <kis_painter.h>
#include <kis_selection.h>
#include <kis_shape_selection.h>
#include <kis_transparency_mask.h>
#include <kis_shape_layer.h>
#include <KoSvgTextShape.h>
#include <KoShapeBackground.h>
#include <KoColorBackground.h>
#include <KoPatternBackground.h>
#include <KoGradientBackground.h>
#include <KoShapeStroke.h>

#include <KoPathShape.h>
#include <KoShapeGroup.h>
#include <KoShapeManager.h>
#include <KoSvgTextShapeMarkupConverter.h>

#include "kis_dom_utils.h"

#include "psd.h"
#include "psd_header.h"
#include "psd_utils.h"

#include "compression.h"

#include <asl/kis_asl_reader_utils.h>
#include <asl/kis_asl_writer_utils.h>
#include <asl/kis_offset_on_exit_verifier.h>
#include <kis_asl_layer_style_serializer.h>
#include <cos/kis_txt2_utls.h>
#include <cos/psd_text_data_converter.h>

PSDLayerMaskSection::PSDLayerMaskSection(const PSDHeader &header)
    : globalInfoSection(header)
    , m_header(header)
{
}

PSDLayerMaskSection::~PSDLayerMaskSection()
{
    qDeleteAll(layers);
}

bool PSDLayerMaskSection::read(PkStream &io)
{
    bool retval = true; // be optimistic! <:-)

    try {
        if (m_header.tiffStyleLayerBlock) {
            switch (m_header.byteOrder) {
            case psd_byte_order::psdLittleEndian:
                retval = readTiffImpl<psd_byte_order::psdLittleEndian>(io);
                break;
            default:
                retval = readTiffImpl(io);
                break;
            }
        } else {
            retval = readPsdImpl(io);
        }
    } catch (KisAslReaderUtils::ASLParseException &e) {
        warnKrita << "WARNING: PSD (emb. pattern):" << e.what();
        retval = false;
    }

    return retval;
}

template<psd_byte_order byteOrder>
bool PSDLayerMaskSection::readLayerInfoImpl(PkStream &io)
{
    std::uint64_t layerInfoSectionSize = 0;
    if (m_header.version == 1) {
        std::uint32_t _layerInfoSectionSize = 0;
        SAFE_READ_EX(byteOrder, io, _layerInfoSectionSize);
        layerInfoSectionSize = _layerInfoSectionSize;
    } else if (m_header.version == 2) {
        SAFE_READ_EX(byteOrder, io, layerInfoSectionSize);
    }

    if (layerInfoSectionSize & 0x1) {
        warnKrita << "WARNING: layerInfoSectionSize is NOT even! Fixing...";
        layerInfoSectionSize++;
    }

    {
        SETUP_OFFSET_VERIFIER(layerInfoSectionTag, io, layerInfoSectionSize, 0);
        dbgFile << "Layer info block size" << layerInfoSectionSize;

        if (layerInfoSectionSize > 0) {
            if (!psdread<byteOrder>(io, nLayers) || nLayers == 0) {
                error = PkString("Could not read number of layers or no layers in image. %1").arg(nLayers);
                return false;
            }

            hasTransparency = nLayers < 0; // first alpha channel is the alpha channel of the projection.
            nLayers = std::abs(nLayers);

            dbgFile << "Number of layers:" << nLayers;
            dbgFile << "Has separate projection transparency:" << hasTransparency;

            for (int i = 0; i < nLayers; ++i) {
                dbgFile << "Going to read layer" << i << "pos" << io.pos();
                dbgFile << "== Enter PSDLayerRecord";
                PSDHeader sanitizedHeader(m_header);
                sanitizedHeader.tiffStyleLayerBlock = false; // disable padding
                std::unique_ptr<PSDLayerRecord> layerRecord(new PSDLayerRecord(sanitizedHeader));
                if (!layerRecord->read(io)) {
                    error = PkString("Could not load layer %1: %2").arg(i).arg(layerRecord->error);
                    return false;
                }
                dbgFile << "== Leave PSDLayerRecord";
                dbgFile << "Finished reading layer" << i << layerRecord->layerName << "blending mode" << layerRecord->blendModeKey << io.pos()
                        << "Number of channels:" << layerRecord->channelInfoRecords.size();
                layers << layerRecord.release();
            }
        }

        // get the positions for the channels belonging to each layer
        for (int i = 0; i < nLayers; ++i) {
            dbgFile << "Going to seek channel positions for layer" << i << "pos" << io.pos();
            if (i > layers.size()) {
                error = PkString("Expected layer %1, but only have %2 layers").arg(i).arg(layers.size());
                return false;
            }

            PSDLayerRecord *layerRecord = layers.at(i);

            for (int j = 0; j < layerRecord->nChannels; ++j) {
                // save the current location so we can jump beyond this block later on.
                std::uint64_t channelStartPos = io.pos();
                dbgFile << "\tReading channel image data for channel" << j << "from pos" << io.pos();

                KIS_ASSERT_RECOVER(j < layerRecord->channelInfoRecords.size())
                {
                    return false;
                }

                ChannelInfo *channelInfo = layerRecord->channelInfoRecords.at(j);

                std::uint16_t compressionType;
                if (!psdread<byteOrder>(io, compressionType)) {
                    error = "Could not read compression type for channel";
                    return false;
                }
                channelInfo->compressionType = static_cast<psd_compression_type>(compressionType);
                dbgFile << "\t\tChannel" << j << "has compression type" << compressionType;

                PkRect channelRect = layerRecord->channelRect(channelInfo);

                // read the rle row lengths;
                if (channelInfo->compressionType == psd_compression_type::RLE) {
                    for (std::int64_t row = 0; row < channelRect.height(); ++row) {
                        // dbgFile << "Reading the RLE byte count position of row" << row << "at pos" << io.pos();

                        std::uint32_t byteCount;
                        if (m_header.version == 1) {
                            std::uint16_t _byteCount;
                            if (!psdread<byteOrder>(io, _byteCount)) {
                                error = PkString("Could not read byteCount for rle-encoded channel");
                                return 0;
                            }
                            byteCount = _byteCount;
                        } else {
                            if (!psdread<byteOrder>(io, byteCount)) {
                                error = PkString("Could not read byteCount for rle-encoded channel");
                                return 0;
                            }
                        }
                        ////dbgFile << "rle byte count" << byteCount;
                        channelInfo->rleRowLengths << byteCount;
                    }
                }

                // we're beyond all the length bytes, rle bytes and whatever, this is the
                // location of the real pixel data
                channelInfo->channelDataStart = io.pos();

                dbgFile << "\t\tstart" << channelStartPos << "data start" << channelInfo->channelDataStart << "data length" << channelInfo->channelDataLength
                        << "pos" << io.pos();

                // make sure we are at the start of the next channel data block
                io.seek(channelStartPos + channelInfo->channelDataLength);

                // this is the length of the actual channel data bytes
                channelInfo->channelDataLength = channelInfo->channelDataLength - (channelInfo->channelDataStart - channelStartPos);

                dbgFile << "\t\tchannel record" << j << "for layer" << i << "with id" << channelInfo->channelId << "starting position"
                        << channelInfo->channelDataStart << "with length" << channelInfo->channelDataLength << "and has compression type"
                        << channelInfo->compressionType;
            }
        }
    }

    return true;
}

bool PSDLayerMaskSection::readPsdImpl(PkStream &io)
{
    dbgFile << "(PSD) reading layer section. Pos:" << io.pos() << "bytes left:" << io.bytesAvailable();

    // https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_21849
    boost::optional<std::uint64_t> layerMaskBlockSize = 0;

    if (m_header.version == 1) {
        std::uint32_t _layerMaskBlockSize = 0;
        SAFE_READ_EX(psd_byte_order::psdBigEndian, io, _layerMaskBlockSize);
        layerMaskBlockSize = _layerMaskBlockSize;
    } else if (m_header.version == 2) {
        SAFE_READ_EX(psd_byte_order::psdBigEndian, io, *layerMaskBlockSize);
    }

    std::int64_t start = io.pos();

    dbgFile << "layer block size" << *layerMaskBlockSize;

    if (*layerMaskBlockSize == 0) {
        dbgFile << "No layer info, so no PSD layers available";
        return true;
    }

    /**
     * PSD files created in some weird web applications may
     * have invalid layer-mask-block-size set. Just do a simple
     * sanity check to catch this case
     */
    if (static_cast<std::int64_t>(*layerMaskBlockSize) > io.bytesAvailable()) {
        warnKrita << "WARNING: invalid layer block size. Got" << *layerMaskBlockSize << "Bytes left" << io.bytesAvailable() << "Triggering a workaround...";

        // just don't use this value for offset recovery at the end
        layerMaskBlockSize = boost::none;
    }

    if (!readLayerInfoImpl(io)) {
        return false;
    }

    dbgFile << "Leftover before additional blocks:" << io.pos() << io.bytesAvailable();

    std::uint32_t globalMaskBlockLength;
    if (!psdread(io, globalMaskBlockLength)) {
        error = "Could not read global mask info block";
        return false;
    }

    dbgFile << "Global mask size:" << globalMaskBlockLength << "(" << io.pos() << io.bytesAvailable() << ")";

    if (globalMaskBlockLength > 0) {
        if (!psdread(io, globalLayerMaskInfo.overlayColorSpace)) {
            error = "Could not read global mask info overlay colorspace";
            return false;
        }

        for (int i = 0; i < 4; ++i) {
            if (!psdread(io, globalLayerMaskInfo.colorComponents[i])) {
                error = PkString("Could not read mask info visualization color component %1").arg(i);
                return false;
            }
        }

        if (!psdread(io, globalLayerMaskInfo.opacity)) {
            error = "Could not read global mask info visualization opacity";
            return false;
        }

        if (!psdread(io, globalLayerMaskInfo.kind)) {
            error = "Could not read global mask info visualization type";
            return false;
        }

        // Global mask must measure at least 13 bytes
        // (excluding the 1 byte compiler enforced padding)
        if (globalMaskBlockLength >= 13) {
            dbgFile << "Padding for global mask block:"
                    << globalMaskBlockLength - 13 << "(" << io.pos() << ")";
            io.skip(static_cast<size_t>(globalMaskBlockLength) - 13);
        }
    }

    // global additional sections

    /**
     * Newer versions of PSD have layers info block wrapped into
     * 'Lr16' or 'Lr32' additional section, while the main block is
     * absent.
     *
     * Here we pass the callback which should be used when such
     * additional section is recognized.
     */
    globalInfoSection.setExtraLayerInfoBlockHandler(
        std::bind(&PSDLayerMaskSection::readLayerInfoImpl<psd_byte_order::psdBigEndian>, this, std::placeholders::_1));

    dbgFile << "Position before starting global info section:" << io.pos();

    globalInfoSection.read(io);

    if (layerMaskBlockSize) {
        /* put us after this section so reading the next section will work even if we mess up */
        io.seek(start + static_cast<std::int64_t>(*layerMaskBlockSize));
    }

    return true;
}

template<psd_byte_order byteOrder>
bool PSDLayerMaskSection::readGlobalMask(PkStream &io)
{
    std::uint32_t globalMaskBlockLength;
    if (!psdread<byteOrder>(io, globalMaskBlockLength)) {
        error = "Could not read global mask info block";
        return false;
    }

    dbgFile << "Global mask size:" << globalMaskBlockLength << "(" << io.pos() << io.bytesAvailable() << ")";

    if (globalMaskBlockLength > 0) {
        if (!psdread<byteOrder>(io, globalLayerMaskInfo.overlayColorSpace)) {
            error = "Could not read global mask info overlay colorspace";
            return false;
        }

        for (int i = 0; i < 4; ++i) {
            if (!psdread<byteOrder>(io, globalLayerMaskInfo.colorComponents[i])) {
                error = PkString("Could not read mask info visualization color component %1").arg(i);
                return false;
            }
        }

        if (!psdread<byteOrder>(io, globalLayerMaskInfo.opacity)) {
            error = "Could not read global mask info visualization opacity";
            return false;
        }

        if (!psdread<byteOrder>(io, globalLayerMaskInfo.kind)) {
            error = "Could not read global mask info visualization type";
            return false;
        }

        dbgFile << "Global mask info: ";
        dbgFile << "\tOverlay:" << globalLayerMaskInfo.overlayColorSpace; // 0
        dbgFile << "\tColor components:" << globalLayerMaskInfo.colorComponents[0] // 65535
                << globalLayerMaskInfo.colorComponents[1] // 0
                << globalLayerMaskInfo.colorComponents[2] // 0
                << globalLayerMaskInfo.colorComponents[3]; // 0
        dbgFile << "\tOpacity:" << globalLayerMaskInfo.opacity; // 50
        dbgFile << "\tKind:" << globalLayerMaskInfo.kind; // 128

        if (globalMaskBlockLength >= 15) {
            io.skip(std::max(globalMaskBlockLength - 15, 0x0U));
        }
    }

    return true;
}

template<psd_byte_order byteOrder>
bool PSDLayerMaskSection::readTiffImpl(PkStream &io)
{
    dbgFile << "(TIFF) reading layer section. Pos:" << io.pos() << "bytes left:" << io.bytesAvailable();

    // TIFF additional sections

    /**
     * Just like PSD, new versions of PSD have layers info block wrapped into
     * 'Lr16' or 'Lr32' additional section, while the main block is
     * absent.
     * Additionally, the global mask info is stored in a separate "LMsk" block.
     *
     * So, instead of having special handling, we just ship everything to the
     * additional layer info block handlers
     */

    globalInfoSection.setExtraLayerInfoBlockHandler(std::bind(&PSDLayerMaskSection::readLayerInfoImpl<byteOrder>, this, std::placeholders::_1));
    globalInfoSection.setUserMaskInfoBlockHandler(std::bind(&PSDLayerMaskSection::readGlobalMask<byteOrder>, this, std::placeholders::_1));

    if (!globalInfoSection.read(io)) {
        dbgFile << "Failed to read TIFF Photoshop blocks!";
        return false;
    }

    const PkStream::pk_int64 remaining = io.bytesAvailable();
    PkByteArray leftover;
    if (remaining > 0) {
        const PkStream::pk_int64 request = std::min(remaining, static_cast<PkStream::pk_int64>(std::numeric_limits<int>::max()));
        std::vector<char> buffer(static_cast<std::size_t>(request));
        const PkStream::pk_int64 peeked = io.peek(buffer.data(), request);
        if (peeked > 0) {
            const int materialized = static_cast<int>(std::min(peeked, request));
            leftover = PkByteArray(buffer.data(), materialized);
        }
    }
    dbgFile << "Leftover data after parsing layer/extra blocks:" << io.pos() << remaining << leftover;

    return true;
}

struct FlattenedNode {
    FlattenedNode()
        : type(RASTER_LAYER)
    {
    }

    KisNodeSP node;

    enum Type { RASTER_LAYER, FOLDER_OPEN, FOLDER_CLOSED, SECTION_DIVIDER };

    Type type;
};

void addBackgroundIfNeeded(KisNodeSP root, PkList<FlattenedNode> &nodes)
{
    KisGroupLayer *group = dynamic_cast<KisGroupLayer *>(root.data());
    if (!group)
        return;

    KoColor projectionColor = group->defaultProjectionColor();
    if (projectionColor.opacityU8() == OPACITY_TRANSPARENT_U8)
        return;

    KisPaintLayerSP layer = new KisPaintLayer(group->image(), PkString("Background"), OPACITY_OPAQUE_U8);

    layer->paintDevice()->setDefaultPixel(projectionColor);

    {
        FlattenedNode item;
        item.node = layer;
        item.type = FlattenedNode::RASTER_LAYER;
        nodes << item;
    }
}

template<typename ShapeList>
void flattenShapes(const KisShapeLayer *parentShapeLayer, const ShapeList &shapes, PkList<FlattenedNode> &nodes) {
    for (KoShape *shape : shapes) {
        const PkString name = toPkString(shape->name()).isEmpty()? PkString("shape ") + PkString("%1").arg(static_cast<int>(nodes.size())): toPkString(shape->name());
        KoShapeGroup *group = dynamic_cast<KoShapeGroup*>(shape);
        if (group) {
            KisGroupLayerSP newGroup(new KisGroupLayer(parentShapeLayer->image(),
                                                       name,
                                                       (1.0-shape->transparency(false))*255,
                                                       parentShapeLayer->colorSpace()));
            newGroup->setVisible(shape->isVisible(false));
            newGroup->setOpacity(shape->transparency(false)*255);
            {
                FlattenedNode item;
                item.node = newGroup;
                item.type = FlattenedNode::SECTION_DIVIDER;
                nodes << item;
            }
            flattenShapes(parentShapeLayer, group->shapes(), nodes);
            {
                FlattenedNode item;
                item.node = newGroup;
                item.type = FlattenedNode::FOLDER_CLOSED;
                nodes << item;
            }
        } else {
            KisShapeLayerSP newLayer(new KisShapeLayer(nullptr,
                                                       parentShapeLayer->image(),
                                                       toQString(name),
                                                       (1.0-shape->transparency(false))*255));
            KoShape *newShape = shape->cloneShape();
            newShape->setTransparency(0.0);
            newShape->setTransformation(shape->absoluteTransformation());
            newLayer->setVisible(newShape->isVisible(false));
            newShape->setVisible(true);
            newLayer->shapeManager()->addShape(newShape, KoShapeManager::AddWithoutRepaint);
            newLayer->addShape(newShape);

            {
                FlattenedNode item;
                item.node = newLayer;
                item.type = FlattenedNode::RASTER_LAYER;
                nodes << item;
            }
        }
    }
}

void flattenNodes(KisNodeSP node, PkList<FlattenedNode> &nodes)
{
    KisNodeSP child = node->firstChild();
    while (child) {
        const bool isLayer = child->inherits("KisLayer");
        const bool isGroupLayer = child->inherits("KisGroupLayer");
        const KisShapeLayer *shapeLayer = dynamic_cast<KisShapeLayer *>(child.data());

        if (isGroupLayer) {
            {
                FlattenedNode item;
                item.node = child;
                item.type = FlattenedNode::SECTION_DIVIDER;
                nodes << item;
            }

            flattenNodes(child, nodes);

            {
                FlattenedNode item;
                item.node = child;
                item.type = FlattenedNode::FOLDER_OPEN;
                nodes << item;
            }
        } else if (shapeLayer) {
            if (shapeLayer->shapes().size() > 1) {
                {
                    FlattenedNode item;
                    item.node = child;
                    item.type = FlattenedNode::SECTION_DIVIDER;
                    nodes << item;
                }
                flattenShapes(shapeLayer, shapeLayer->shapes(), nodes);
                {
                    FlattenedNode item;
                    item.node = child;
                    item.type = FlattenedNode::FOLDER_CLOSED;
                    nodes << item;
                }
            } else {
                FlattenedNode item;
                item.node = child;
                item.type = FlattenedNode::RASTER_LAYER;
                nodes << item;
            }
        } else if (isLayer) {
            FlattenedNode item;
            item.node = child;
            item.type = FlattenedNode::RASTER_LAYER;
            nodes << item;
        }

        child = child->nextSibling();
    }
}

KisNodeSP findOnlyTransparencyMask(KisNodeSP node, FlattenedNode::Type type)
{
    if (type != FlattenedNode::FOLDER_OPEN && type != FlattenedNode::FOLDER_CLOSED && type != FlattenedNode::RASTER_LAYER) {
        return 0;
    }

    KisLayer *layer = dynamic_cast<KisLayer *>(node.data());
    PkList<KisEffectMaskSP> masks = layer->effectMasks();

    if (masks.size() != 1)
        return 0;

    KisEffectMaskSP onlyMask = masks.first();
    return onlyMask->inherits("KisTransparencyMask") ? onlyMask : 0;
}

PkXmlDocument fetchLayerStyleXmlData(KisNodeSP node)
{
    const KisLayer *layer = dynamic_cast<KisLayer *>(node.data());
    KisPSDLayerStyleSP layerStyle = layer->layerStyle();

    if (!layerStyle)
        return PkXmlDocument();

    KisAslLayerStyleSerializer serializer;
    serializer.setStyles(PkVector<KisPSDLayerStyleSP>() << layerStyle);
    return serializer.formPsdXmlDocument();
}

inline PkXmlNode findNodeByKey(const PkString &key, PkXmlNode parent)
{
    return KisDomUtils::findElementByAttribute(parent, "node", "key", key);
}

void mergePatternsXMLSection(const PkXmlDocument &src, PkXmlDocument &dst)
{
    PkXmlNode srcPatternsNode = findNodeByKey(ResourceType::Patterns, src.documentElement());
    PkXmlNode dstPatternsNode = findNodeByKey(ResourceType::Patterns, dst.documentElement());

    if (srcPatternsNode.isNull())
        return;
    if (dstPatternsNode.isNull()) {
        dst = src;
        return;
    }

    KIS_ASSERT_RECOVER_RETURN(!srcPatternsNode.isNull());
    KIS_ASSERT_RECOVER_RETURN(!dstPatternsNode.isNull());

    PkXmlNode node = srcPatternsNode.firstChild();
    while (!node.isNull()) {
        PkXmlNode importedNode = dst.importNode(node, true);
        KIS_ASSERT_RECOVER_RETURN(!importedNode.isNull());

        dstPatternsNode.appendChild(importedNode);
        node = node.nextSibling();
    }
}

bool PSDLayerMaskSection::write(PkStream &io, KisNodeSP rootLayer, psd_compression_type compressionType)
{
    bool retval = true;

    try {
        if (m_header.tiffStyleLayerBlock) {
            switch (m_header.byteOrder) {
            case psd_byte_order::psdLittleEndian:
                writeTiffImpl<psd_byte_order::psdLittleEndian>(io, rootLayer, compressionType);
                break;
            default:
                writeTiffImpl(io, rootLayer, compressionType);
                break;
            }
        } else {
            writePsdImpl(io, rootLayer, compressionType);
        }
    } catch (KisAslWriterUtils::ASLWriteException &e) {
        error = PREPEND_METHOD(e.what());
        retval = false;
    }

    return retval;
}

void PSDLayerMaskSection::writePsdImpl(PkStream &io, KisNodeSP rootLayer, psd_compression_type compressionType)
{
    dbgFile << "Writing layer section";

    globalInfoSection.txt2Data = KisTxt2Utils::defaultTxt2();
    //int textCount = 0;

    // Build the whole layer structure
    PkList<FlattenedNode> nodes;
    addBackgroundIfNeeded(rootLayer, nodes);
    flattenNodes(rootLayer, nodes);

    if (nodes.isEmpty()) {
        throw KisAslWriterUtils::ASLWriteException("Could not find paint layers to save");
    }

    {
        KisAslWriterUtils::OffsetStreamPusher<std::uint32_t, psd_byte_order::psdBigEndian> layerAndMaskSectionSizeTag(io, 2);
        PkXmlDocument mergedPatternsXmlDoc;

        {
            KisAslWriterUtils::OffsetStreamPusher<std::uint32_t, psd_byte_order::psdBigEndian> layerInfoSizeTag(io, 2);

            {
                // number of layers (negative, because krita always has alpha)
                const std::int16_t layersSize = static_cast<std::int16_t>(-nodes.size());
                SAFE_WRITE_EX(psd_byte_order::psdBigEndian, io, layersSize);

                dbgFile << "Number of layers" << layersSize << "at" << io.pos();
            }

            // Layer records section
            for (const FlattenedNode &item : nodes) {
                KisNodeSP node = item.node;

                PSDLayerRecord *layerRecord = new PSDLayerRecord(m_header);
                layers.append(layerRecord);

                KisNodeSP onlyTransparencyMask = findOnlyTransparencyMask(node, item.type);
                PkRect maskRect = onlyTransparencyMask ? onlyTransparencyMask->paintDevice()->exactBounds() : PkRect();

                const bool nodeVisible = node->visible();
                const KoColorSpace *colorSpace = node->colorSpace();
                const std::uint8_t nodeOpacity = node->opacity();
                const std::uint8_t nodeClipping = 0;
                const int nodeLabelColor = node->colorLabelIndex();
                const KisPaintLayer *paintLayer = dynamic_cast<KisPaintLayer *>(node.data());
                const bool alphaLocked = (paintLayer && paintLayer->alphaLocked());
                const PkString nodeCompositeOp = node->compositeOpId();

                const KisGroupLayer *groupLayer = dynamic_cast<KisGroupLayer *>(node.data());
                const bool nodeIsPassThrough = groupLayer && groupLayer->passThroughMode();

                const KisGeneratorLayer *fillLayer = dynamic_cast<KisGeneratorLayer *>(node.data());
                PkXmlDocument fillConfig;
                psd_fill_type fillType = psd_fill_solid_color;
                if (fillLayer) {
                    PkString generatorName = fillLayer->filter()->name();
                    if (generatorName == "color") {
                        psd_layer_solid_color fill;
                        if (fill.loadFromConfig(fillLayer->filter())) {
                            if (node->image()) {
                                fill.cs = node->image()->colorSpace();
                            } else {
                                fill.cs = node->colorSpace();
                            }
                            fillConfig = fill.getASLXML();
                            fillType = psd_fill_solid_color;
                        }
                    } else if (generatorName == "gradient") {
                        psd_layer_gradient_fill fill;
                        fill.imageWidth = node->image()->width();
                        fill.imageHeight = node->image()->height();
                        if (fill.loadFromConfig(fillLayer->filter())) {
                            fillConfig = fill.getASLXML();
                            fillType = psd_fill_gradient;
                        }
                    } else if (generatorName == "pattern") {

                        psd_layer_pattern_fill fill;
                        if (fill.loadFromConfig(fillLayer->filter())) {
                            if (fill.pattern) {
                                KisAslXmlWriter w;
                                w.enterList(ResourceType::Patterns);
                                PkString uuid = w.writePattern("", fill.pattern);
                                w.leaveList();
                                mergedPatternsXmlDoc = w.document();
                                fill.patternID = uuid;
                                fillConfig = fill.getASLXML();
                                fillType = psd_fill_pattern;
                            }
                        }

                    }
                    // And if anything else, it cannot be stored as a PSD fill layer.
                }

                double vectorWidth = rootLayer->image()? rootLayer->image()->width() / rootLayer->image()->xRes(): 1;
                double vectorHeight = rootLayer->image()? rootLayer->image()->height() / rootLayer->image()->yRes(): 1;
                PkTransform FlaketoPixels = PkTransform::fromScale(rootLayer->image()->xRes(), rootLayer->image()->yRes());

                const KisShapeLayer *shapeLayer = dynamic_cast<KisShapeLayer *>(node.data());
                psd_layer_type_shape textData;
                psd_vector_mask vectorMask;
                PkXmlDocument strokeData;
                PkXmlDocument vogkData;

                if (shapeLayer && !shapeLayer->isFakeNode()) {
                    // only store the first shape.
                    if (shapeLayer->shapes().size() == 1) {
                        KoSvgTextShape * text = dynamic_cast<KoSvgTextShape*>(shapeLayer->shapes().first());
                        if (text) {
                            PsdTextDataConverter convert;
                            KoSvgTextShapeMarkupConverter svgConverter(text);
                            PK_QSTRING_ qtSvgText;
                            PK_QSTRING_ qtStyles;
                            svgConverter.convertToSvg(&qtSvgText, &qtStyles);
                            const PkString svgtext = toPkString(qtSvgText);
                            // unsure about the boundingBox, needs more research.
                            textData.boundingBox = toPkRectF(text->boundingRect().normalized());
                            if (text->shapesInside().isEmpty()) {
                                // Scale bbox to inline
                                const KoSvgText::AutoValue inlineSizeProp =
                                    text->textProperties().property(KoSvgTextProperties::InlineSizeId).value<KoSvgText::AutoValue>();
                                if (!inlineSizeProp.isAuto) {
                                    if (text->writingMode() == KoSvgText::HorizontalTB) {
                                        textData.boundingBox.setWidth(inlineSizeProp.customValue);
                                    } else {
                                        textData.boundingBox.setHeight(inlineSizeProp.customValue);
                                    }
                                }
                            }
                            textData.bounds = toPkRectF(text->outlineRect().normalized());

                            bool res = convert.convertToPSDTextEngineData(svgtext,
                                                                         textData.bounds,
                                                                         toPkList(text->shapesInside()),
                                                                         globalInfoSection.txt2Data,
                                                                         textData.textIndex,
                                                                         textData.text,
                                                                         textData.isHorizontal,
                                                                         FlaketoPixels);
                            if (!res && !convert.errors().isEmpty()) {
                                for (const PkString &message : convert.errors()) {
                                    qWarning() << message;
                                }
                            }
                            for (const PkString &message : convert.warnings()) {
                                dbgFile << message;
                            }
                            textData.engineData = KisTxt2Utils::tyShFromTxt2(globalInfoSection.txt2Data, FlaketoPixels.mapRect(textData.boundingBox), textData.textIndex);
                            //textCount += 1;
                            if (!text->shapesInside().isEmpty()) {
                                textData.bounds = toPkRectF(text->outlineRect().normalized());
                            }
                            if (!textData.bounds.isEmpty()) {
                                textData.boundingBox = FlaketoPixels.mapRect(textData.boundingBox);
                                textData.bounds = FlaketoPixels.mapRect(textData.bounds);
                            } else {
                                textData.boundingBox = PkRectF();
                            }
                            textData.transform = FlaketoPixels.inverted() * toPkTransform(text->absoluteTransformation()) * FlaketoPixels;
                        } else {
                            KoPathShape *pathShape = dynamic_cast<KoPathShape*>(shapeLayer->shapes().first());
                            if (pathShape){
                                layerRecord->addPathShapeToPSDPath(vectorMask.path, pathShape, vectorWidth, vectorHeight);

                                // Right now only saving rect and ellipse when they are 'simple', as the actual parametric
                                // shapes themselves are plugins, so we cannot include them and access the object data.
                                if ((pathShape->pathShapeId() == "RectangleShape" || pathShape->pathShapeId() == "EllipseShape")
                                        && pathShape->pointCount() == 4) {
                                    psd_vector_origination_data data;
                                    data.originType = data.typeToName.key(toPkString(pathShape->pathShapeId()), 1);
                                    const PkTransform pathTransform = toPkTransform(pathShape->absoluteTransformation());
                                    PkPolygonF poly = pathTransform.map(toPkRectF(pathShape->outlineRect()));
                                    data.originShapeBBox = poly.boundingRect();
                                    data.originBoxCorners = poly;
                                    data.transform = pathTransform;
                                    vogkData = data.getASL();
                                }
                                KoColorBackground *b = dynamic_cast<KoColorBackground *>(pathShape->background().data());
                                KoGradientBackground *g = dynamic_cast<KoGradientBackground *>(pathShape->background().data());
                                KoPatternBackground *p = dynamic_cast<KoPatternBackground *>(pathShape->background().data());
                                if (b) {
                                    psd_layer_solid_color fill;

                                    if (node->image()) {
                                        fill.cs = node->image()->colorSpace();
                                    } else {
                                        fill.cs = node->colorSpace();
                                    }
                                    fill.setColor(KoColor(toPkColor(b->color()), fill.cs));
                                    fillConfig = fill.getASLXML();
                                    fillType = psd_fill_solid_color;
                                } else if (g) {
                                    psd_layer_gradient_fill fill;
                                    fill.setFromQGradient(g->gradient());
                                    fillConfig = fill.getASLXML();
                                    fillType = psd_fill_gradient;
                                } else if (p) {
                                    psd_layer_pattern_fill fill;
                                    fillConfig = fill.getASLXML();
                                    fillType = psd_fill_pattern;
                                } else if (!pathShape->background()) {
                                    psd_layer_solid_color fill;

                                    if (node->image()) {
                                        fill.cs = node->image()->colorSpace();
                                    } else {
                                        fill.cs = node->colorSpace();
                                    }
                                    fill.setColor(KoColor(Qt::transparent, fill.cs));
                                    fillConfig = fill.getASLXML();
                                    fillType = psd_fill_solid_color;
                                }
                                KoShapeStrokeSP shapeStroke = pathShape->stroke().dynamicCast<KoShapeStroke>();
                                if (shapeStroke) {
                                    psd_vector_stroke_data strokeDataStruct;
                                    strokeDataStruct.loadFromShapeStroke(shapeStroke);
                                    strokeDataStruct.strokeEnabled = shapeStroke->isVisible();
                                    strokeDataStruct.fillEnabled = pathShape->background()? true: false;
                                    strokeDataStruct.resolution = node->image()->xRes()*72.0;
                                    strokeData = strokeDataStruct.getASLXML();
                                }
                            }
                        }
                    }
                }

                PkXmlDocument stylesXmlDoc = fetchLayerStyleXmlData(node);

                if (mergedPatternsXmlDoc.isNull() && !stylesXmlDoc.isNull()) {
                    mergedPatternsXmlDoc = stylesXmlDoc;
                } else if (!mergedPatternsXmlDoc.isNull() && !stylesXmlDoc.isNull()) {
                    mergePatternsXMLSection(stylesXmlDoc, mergedPatternsXmlDoc);
                }

                bool nodeIrrelevant = false;
                PkString nodeName;
                KisPaintDeviceSP layerContentDevice;
                psd_section_type sectionType;

                if (item.type == FlattenedNode::RASTER_LAYER) {
                    nodeIrrelevant = false;
                    nodeName = node->name();
                    layerContentDevice = onlyTransparencyMask ? node->original() : node->projection();

                    /**
                     * For fill layers we save their internal selection as a separate transparency mask
                     */
                    if (fillLayer) {
                        bool transparency = KisPainter::checkDeviceHasTransparency(node->paintDevice());
                        bool semiOpacity = node->paintDevice()->defaultPixel().opacityU8() < OPACITY_OPAQUE_U8;
                        if (transparency || semiOpacity) {
                            KisSelectionSP selection = fillLayer->internalSelection();
                            if(selection) {
                                if(selection->hasNonEmptyShapeSelection()) {
                                    KisShapeSelection* shapeSelection = dynamic_cast<KisShapeSelection*>(selection->shapeSelection());
                                    if (shapeSelection) {
                                        for (KoShape *shape : shapeSelection->shapes()) {
                                            KoPathShape *pathShape = dynamic_cast<KoPathShape*>(shape);
                                            if (pathShape){
                                                layerRecord->addPathShapeToPSDPath(vectorMask.path, pathShape, vectorWidth, vectorHeight);

                                            }
                                        }
                                    }
                                }
                            }
                            layerContentDevice = node->original();
                            onlyTransparencyMask = node;
                            maskRect = onlyTransparencyMask->paintDevice()->exactBounds();
                        }
                    } else {
                        KisTransparencyMask *mask = dynamic_cast<KisTransparencyMask*>(onlyTransparencyMask.data());
                        if (mask) {
                        KisSelectionSP selection = mask->selection();
                        if(selection) {
                            if(selection->hasNonEmptyShapeSelection()) {
                                KisShapeSelection* shapeSelection = dynamic_cast<KisShapeSelection*>(selection->shapeSelection());
                                if (shapeSelection) {
                                    for (KoShape *shape : shapeSelection->shapes()) {
                                        KoPathShape *pathShape = dynamic_cast<KoPathShape*>(shape);
                                        if (pathShape){
                                            layerRecord->addPathShapeToPSDPath(vectorMask.path, pathShape, vectorWidth, vectorHeight);
                                        }
                                    }
                                }
                            }
                        }
                        }
                    }
                    sectionType = psd_other;
                } else {
                    nodeIrrelevant = true;
                    nodeName = item.type == FlattenedNode::SECTION_DIVIDER ? PkString("</Layer group>") : node->name();
                    layerContentDevice = 0;
                    sectionType = item.type == FlattenedNode::SECTION_DIVIDER ? psd_bounding_divider
                        : item.type == FlattenedNode::FOLDER_OPEN             ? psd_open_folder
                                                                              : psd_closed_folder;
                }

                // === no access to node anymore

                PkRect layerRect;

                if (layerContentDevice) {
                    PkRect rc = layerContentDevice->exactBounds();
                    rc = rc.normalized();

                    // keep to the max of photoshop's capabilities
                    if (rc.width() > 30000)
                        rc.setWidth(30000);
                    if (rc.height() > 30000)
                        rc.setHeight(30000);

                    layerRect = rc;
                }

                layerRecord->top = layerRect.y();
                layerRecord->left = layerRect.x();
                layerRecord->bottom = layerRect.y() + layerRect.height();
                layerRecord->right = layerRect.x() + layerRect.width();

                // colors + alpha channel
                // note: transparency mask not included
                layerRecord->nChannels = static_cast<std::uint16_t>(colorSpace->colorChannelCount() + 1);

                ChannelInfo *info = new ChannelInfo;
                info->channelId = -1; // For the alpha channel, which we always have in Krita, and should be saved first in
                layerRecord->channelInfoRecords << info;

                // the rest is in display order: rgb, cmyk, lab...
                for (std::int16_t i = 0; i < (int)colorSpace->colorChannelCount(); ++i) {
                    info = new ChannelInfo;
                    info->channelId = i; // 0 for red, 1 = green, etc
                    layerRecord->channelInfoRecords << info;
                }

                layerRecord->blendModeKey = composite_op_to_psd_blendmode(nodeCompositeOp);
                layerRecord->isPassThrough = nodeIsPassThrough;
                layerRecord->opacity = nodeOpacity;
                layerRecord->clipping = nodeClipping;

                layerRecord->labelColor = nodeLabelColor;

                layerRecord->transparencyProtected = alphaLocked;
                layerRecord->visible = nodeVisible;
                layerRecord->irrelevant = nodeIrrelevant;

                layerRecord->layerName = nodeName.isEmpty() ? PkString("Unnamed Layer") : nodeName;

                layerRecord->fillType = fillType;
                layerRecord->fillConfig = fillConfig;

                layerRecord->vectorMask = vectorMask;
                layerRecord->vectorStroke = strokeData;
                layerRecord->vectorOriginationData = vogkData;

                layerRecord->textShape = textData;

                layerRecord->write(io, layerContentDevice, onlyTransparencyMask, maskRect, sectionType, stylesXmlDoc, node->inherits("KisGroupLayer"));
            }

            dbgFile << "start writing layer pixel data" << io.pos();

            // Now save the pixel data
            for (PSDLayerRecord *layerRecord : layers) {
                layerRecord->writePixelData(io, compressionType);
            }
        }

        {
            // write the global layer mask info -- which is empty
            const std::uint32_t globalMaskSize = 0;
            SAFE_WRITE_EX(psd_byte_order::psdBigEndian, io, globalMaskSize);
        }

        globalInfoSection.writePattBlockEx(io, mergedPatternsXmlDoc);

#if 0
        /**
         * We're currently not writing the Txt2 data itself as it doesn't
         * result in correct PSDs. There's three possible culprits for this:
         *
         * 1. PSD perhaps requires the data to be stored in a specific order.
         *    The 'uncompressKeys' function in kis_txt2_utls gives an indication of this order.
         * 2. PSD requires the Strikes for each text object to be written.
         *    This is the most likely cause. The strikes object however consists of data for
         *    every line, segment, and character, with positioning, bounding boxes and even
         *    precise font glyph indices for each character. This is more or less a cached
         *    version of the layout data of the text shape, and we don't have that kind of access
         *    of the text shape data right now.
         * 3. Something else. The Txt2 data is huge and therefore it is hard to figure out
         *    where things might be going wrong.
         *
         * In practice, this means Krita won't be able to store OpenType feature data as well
         * as path shapes for either text-in-shape or text-on-path.
         */
        if (textCount > 0) {
            globalInfoSection.writeTxt2BlockEx(io, globalInfoSection.txt2Data);
        }
#endif
    }
}

template<psd_byte_order byteOrder>
void PSDLayerMaskSection::writeTiffImpl(PkStream &io, KisNodeSP rootLayer, psd_compression_type compressionType)
{
    dbgFile << "(TIFF) Writing layer section";

    // Build the whole layer structure
    PkList<FlattenedNode> nodes;
    addBackgroundIfNeeded(rootLayer, nodes);
    flattenNodes(rootLayer, nodes);

    if (nodes.isEmpty()) {
        throw KisAslWriterUtils::ASLWriteException("Could not find paint layers to save");
    }

    {
        PkXmlDocument mergedPatternsXmlDoc;

        {
            KisAslWriterUtils::writeFixedString<byteOrder>("8BIM", io);
            KisAslWriterUtils::writeFixedString<byteOrder>("Layr", io);

            KisAslWriterUtils::OffsetStreamPusher<std::uint32_t, byteOrder> layerAndMaskSectionSizeTag(io, 4);
            // number of layers (negative, because krita always has alpha)
            const std::int16_t layersSize = nodes.size();
            SAFE_WRITE_EX(byteOrder, io, layersSize);

            dbgFile << "Number of layers" << layersSize << "at" << io.pos();

            // Layer records section
            for (const FlattenedNode &item : nodes) {
                KisNodeSP node = item.node;

                PSDLayerRecord *layerRecord = new PSDLayerRecord(m_header);
                layers.append(layerRecord);

                const bool nodeVisible = node->visible();
                const KoColorSpace *colorSpace = node->colorSpace();
                const std::uint8_t nodeOpacity = node->opacity();
                const std::uint8_t nodeClipping = 0;
                const int nodeLabelColor = node->colorLabelIndex();
                const KisPaintLayer *paintLayer = dynamic_cast<KisPaintLayer *>(node.data());
                const bool alphaLocked = (paintLayer && paintLayer->alphaLocked());
                const PkString nodeCompositeOp = node->compositeOpId();

                const KisGroupLayer *groupLayer = dynamic_cast<KisGroupLayer *>(node.data());
                const bool nodeIsPassThrough = groupLayer && groupLayer->passThroughMode();

                PkXmlDocument stylesXmlDoc = fetchLayerStyleXmlData(node);

                if (mergedPatternsXmlDoc.isNull() && !stylesXmlDoc.isNull()) {
                    mergedPatternsXmlDoc = stylesXmlDoc;
                } else if (!mergedPatternsXmlDoc.isNull() && !stylesXmlDoc.isNull()) {
                    mergePatternsXMLSection(stylesXmlDoc, mergedPatternsXmlDoc);
                }

                bool nodeIrrelevant = false;
                PkString nodeName;
                KisPaintDeviceSP layerContentDevice;
                psd_section_type sectionType;

                if (item.type == FlattenedNode::RASTER_LAYER) {
                    nodeIrrelevant = false;
                    nodeName = node->name();
                    layerContentDevice = node->projection();
                    sectionType = psd_other;
                } else {
                    nodeIrrelevant = true;
                    nodeName = item.type == FlattenedNode::SECTION_DIVIDER ? PkString("</Layer group>") : node->name();
                    layerContentDevice = 0;
                    sectionType = item.type == FlattenedNode::SECTION_DIVIDER ? psd_bounding_divider
                        : item.type == FlattenedNode::FOLDER_OPEN             ? psd_open_folder
                                                                              : psd_closed_folder;
                }

                // === no access to node anymore

                PkRect layerRect;

                if (layerContentDevice) {
                    PkRect rc = layerContentDevice->exactBounds();
                    rc = rc.normalized();

                    // keep to the max of photoshop's capabilities
                    // XXX: update this to PSB
                    if (rc.width() > 30000)
                        rc.setWidth(30000);
                    if (rc.height() > 30000)
                        rc.setHeight(30000);

                    layerRect = rc;
                }

                layerRecord->top = layerRect.y();
                layerRecord->left = layerRect.x();
                layerRecord->bottom = layerRect.y() + layerRect.height();
                layerRecord->right = layerRect.x() + layerRect.width();

                // colors + alpha channel
                // note: transparency mask not included
                layerRecord->nChannels = static_cast<std::uint16_t>(colorSpace->colorChannelCount() + 1);

                ChannelInfo *info = new ChannelInfo;
                info->channelId = -1; // For the alpha channel, which we always have in Krita, and should be saved first in
                layerRecord->channelInfoRecords << info;

                // the rest is in display order: rgb, cmyk, lab...
                for (std::uint32_t i = 0; i < colorSpace->colorChannelCount(); ++i) {
                    info = new ChannelInfo;
                    info->channelId = static_cast<std::int16_t>(i); // 0 for red, 1 = green, etc
                    layerRecord->channelInfoRecords << info;
                }

                layerRecord->blendModeKey = composite_op_to_psd_blendmode(nodeCompositeOp);
                layerRecord->isPassThrough = nodeIsPassThrough;
                layerRecord->opacity = nodeOpacity;
                layerRecord->clipping = nodeClipping;

                layerRecord->transparencyProtected = alphaLocked;
                layerRecord->visible = nodeVisible;
                layerRecord->irrelevant = nodeIrrelevant;
                layerRecord->labelColor = nodeLabelColor;

                layerRecord->layerName = nodeName.isEmpty() ? PkString("Unnamed Layer") : nodeName;

                layerRecord->write(io, layerContentDevice, nullptr, PkRect(), sectionType, stylesXmlDoc, node->inherits("KisGroupLayer"));
            }

            dbgFile << "start writing layer pixel data" << io.pos();

            // Now save the pixel data
            for (PSDLayerRecord *layerRecord : layers) {
                layerRecord->writePixelData(io, compressionType);
            }
        }

        // {
        //     // write the global layer mask info -- which is NOT empty but fixed
        //     KisAslWriterUtils::writeFixedString<byteOrder>("8BIM", io);
        //     KisAslWriterUtils::writeFixedString<byteOrder>("LMsk", io);

        //     KisAslWriterUtils::OffsetStreamPusher<std::uint32_t, byteOrder> layerAndMaskSectionSizeTag(io, 4);
        //     // https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577411_22664
        //     psdwrite<byteOrder>(io, std::uint16_t(0));     // CS: RGB
        //     psdwrite<byteOrder>(io, std::uint16_t(65535)); // Pure red verification
        //     psdwrite<byteOrder>(io, std::uint16_t(0));
        //     psdwrite<byteOrder>(io, std::uint16_t(0));
        //     psdwrite<byteOrder>(io, std::uint16_t(0));
        //     psdwrite<byteOrder>(io, std::uint16_t(50)); // opacity
        //     psdwrite<byteOrder>(io, std::uint16_t(128)); // kind
        // }

        globalInfoSection.writePattBlockEx(io, mergedPatternsXmlDoc);
    }
}
