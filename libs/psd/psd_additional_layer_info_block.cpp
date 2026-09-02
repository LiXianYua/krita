/*
 *  SPDX-FileCopyrightText: 2014 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtGlobal>

#include "psd_additional_layer_info_block.h"
#include "psd.h"

#include <PkXmlDocument.h>

#include <QPointF>
#include <QRectF>

#include <asl/kis_offset_on_exit_verifier.h>

#include <asl/kis_asl_patterns_writer.h>
#include <asl/kis_asl_reader.h>
#include <asl/kis_asl_reader_utils.h>
#include <asl/kis_asl_writer.h>
#include <asl/kis_asl_writer_utils.h>
#include <cos/kis_txt2_utls.h>


PsdAdditionalLayerInfoBlock::PsdAdditionalLayerInfoBlock(const PSDHeader &header)
    : m_header(header)
    , sectionDividerType(psd_other)
{
}

void PsdAdditionalLayerInfoBlock::setExtraLayerInfoBlockHandler(ExtraLayerInfoBlockHandler handler)
{
    m_layerInfoBlockHandler = handler;
}

void PsdAdditionalLayerInfoBlock::setUserMaskInfoBlockHandler(UserMaskInfoBlockHandler handler)
{
    m_userMaskBlockHandler = handler;
}

bool PsdAdditionalLayerInfoBlock::read(PkStream &io)
{
    bool result = true;

    try {
        switch (m_header.byteOrder) {
        case psd_byte_order::psdLittleEndian:
            readImpl<psd_byte_order::psdLittleEndian>(io);
            break;
        default:
            readImpl(io);
            break;
        }
    } catch (KisAslReaderUtils::ASLParseException &e) {
        error = e.what();
        result = false;
    }

    return result;
}

template<psd_byte_order byteOrder>
void PsdAdditionalLayerInfoBlock::readImpl(PkStream &io)
{
    using namespace KisAslReaderUtils;

    PkStringList longBlocks;
    if (m_header.version > 1) {
        longBlocks << "LMsk"
                   << "Lr16"
                   << "Lr32"
                   << "Layr"
                   << "Mt16"
                   << "Mt32"
                   << "Mtrn"
                   << "Alph"
                   << "FMsk"
                   << "lnk2"
                   << "FEid"
                   << "FXid"
                   << "PxSD";
    }

    while (!io.atEnd()) {
        {
            const std::array<std::uint8_t, 4> refSignature1 = {'8', 'B', 'I', 'M'}; // '8BIM' in big-endian
            const std::array<std::uint8_t, 4> refSignature2 = {'8', 'B', '6', '4'}; // '8B64' in big-endian

            if (!TRY_READ_SIGNATURE_2OPS_EX<byteOrder>(io, refSignature1, refSignature2)) {
                break;
            }
        }

        PkString key = readFixedString<byteOrder>(io);
        dbgFile << "found info block with key" << key << "(" << io.pos() << ")";

        std::uint64_t blockSize = GARBAGE_VALUE_MARK;
        if (longBlocks.contains(key)) {
            SAFE_READ_EX(byteOrder, io, blockSize);
        } else {
            std::uint32_t size32;
            SAFE_READ_EX(byteOrder, io, size32);
            blockSize = size32;
        }

        // Since TIFF headers are padded to multiples of 4,
        // staving them off here is way easier.
        if (m_header.tiffStyleLayerBlock) {
            if (blockSize % 4U) {
                dbgFile << "(TIFF) WARNING: current block size is NOT a multiple of 4! Fixing...";
                blockSize += (4U - blockSize % 4U);
            }
        }

        dbgFile << "info block size" << blockSize << "(" << io.pos() << ")";

        if (blockSize == 0)
            continue;

        // offset verifier will correct the position on the exit from
        // current namespace, including 'continue', 'return' and
        // exceptions.
        SETUP_OFFSET_VERIFIER(infoBlockEndVerifier, io, blockSize, 0);

        if (keys.contains(key)) {
            error = "Found duplicate entry for key ";
            continue;
        }
        keys << key;

        // TODO: Loading of 32 bit files is not supported yet
        if (key == "Lr16" /* || key == "Lr32"*/) {
            if (m_layerInfoBlockHandler) {
                int offset = m_header.version > 1 ? 8 : 4;
                dbgFile << "Offset for block handler: " << io.pos() << offset;
                io.seek(io.pos() - offset);
                m_layerInfoBlockHandler(io);
            }
        } else if (key == "Layr") {
            if (m_header.tiffStyleLayerBlock && m_layerInfoBlockHandler) {
                int offset = m_header.version > 1 ? 8 : 4;
                dbgFile << "(TIFF) Offset for block handler: " << io.pos() << offset;
                io.seek(io.pos() - offset);
                m_layerInfoBlockHandler(io);
            }
        } else if (key == "SoCo") {
            // Solid Color
            fillConfig = KisAslReader::readFillLayer(io, byteOrder);
            fillType = psd_fill_solid_color;
        } else if (key == "GdFl") {
            // Gradient Fill
            fillConfig = KisAslReader::readFillLayer(io, byteOrder);
            fillType = psd_fill_gradient;
        } else if (key == "PtFl") {
            // Pattern Fill
            fillConfig = KisAslReader::readFillLayer(io, byteOrder);
            fillType = psd_fill_pattern;
        } else if (key == "brit") {
        } else if (key == "levl") {
        } else if (key == "curv") {
        } else if (key == "expA") {
        } else if (key == "vibA") {
        } else if (key == "hue") {
        } else if (key == "hue2") {
        } else if (key == "blnc") {
        } else if (key == "blwh") {
        } else if (key == "phfl") {
        } else if (key == "mixr") {
        } else if (key == "clrL") {
        } else if (key == "nvrt") {
        } else if (key == "post") {
        } else if (key == "thrs") {
        } else if (key == "selc") {
        } else if (key == "lrFX") {
            // deprecated! use lfx2 instead!
        } else if (key == "tySh") {
        } else if (key == "luni") {
            // get the unicode layer name
            unicodeLayerName = readUnicodeString<byteOrder>(io);
            dbgFile << "\t" << "unicodeLayerName" << unicodeLayerName;
        } else if (key == "lyid") {
            std::uint32_t id;
            psdread<byteOrder>(io, id);
            dbgFile << "\t" << "layer ID:" << id;
        } else if (key == "lfx2" || key == "lfxs") {
            // lfxs is a special variant of layer styles for group layers
            layerStyleXml = KisAslReader::readLfx2PsdSection(io, byteOrder);
        } else if (key == "Patt" || key == "Pat2" || key == "Pat3") {
            PkXmlDocument pattern = KisAslReader::readPsdSectionPattern(io, blockSize, byteOrder);
            embeddedPatterns << pattern;
        } else if (key == "Anno") {
        } else if (key == "clbl") {
        } else if (key == "infx") {
        } else if (key == "knko") {
        } else if (key == "spf") {
        } else if (key == "lclr") {
            // layer label color.
            std::uint16_t col1 = 0;
            std::uint16_t col2 = 0;
            std::uint16_t col3 = 0;
            std::uint16_t col4 = 0;
            psdread<byteOrder>(io, col1);
            psdread<byteOrder>(io, col2);
            psdread<byteOrder>(io, col3);
            psdread<byteOrder>(io, col4);
            dbgFile << "\t" << "layer color:" << col1 << col2 << col3 << col4;
            labelColor = col1;
        } else if (key == "fxrp") {
        } else if (key == "grdm") {
        } else if (key == "lsct") {
            std::uint32_t dividerType = GARBAGE_VALUE_MARK;
            SAFE_READ_EX(byteOrder, io, dividerType);
            this->sectionDividerType = (psd_section_type)dividerType;

            dbgFile << "Reading \"lsct\" block:";
            dbgFile << ppVar(blockSize);
            dbgFile << ppVar(dividerType);

            if (blockSize >= 12) {
                std::uint32_t lsctSignature = GARBAGE_VALUE_MARK;
                const std::uint32_t refSignature1 = 0x3842494D; // '8BIM' in little-endian
                SAFE_READ_SIGNATURE_EX(byteOrder, io, lsctSignature, refSignature1);

                this->sectionDividerBlendMode = readFixedString<byteOrder>(io);

                dbgFile << ppVar(this->sectionDividerBlendMode);
            }

            // Animation
            if (blockSize >= 14) {
                /**
                 * "I don't care
                 *  I don't care, no... !" (c)
                 */
            }

        } else if (key == "brst") {
        } else if (key == "vmsk" || key == "vsms") { // If key is "vsms" then we are writing for (Photoshop CS6) and the document will have a "vscg" key
            std::uint32_t version; // ( = 3 for Photoshop 6.0)
            psdread<byteOrder>(io, version);

            std::uint32_t flags;
            psdread<byteOrder>(io, flags);
            // read the flags.
            vectorMask.invert  = flags & 1? true: false;
            vectorMask.notLink = flags & 2? true: false;
            vectorMask.disable = flags & 4? true: false;

            std::uint64_t currentPos = 8;
            psd_path_sub_path currentPath;
            bool firstPath = true;

            while (currentPos < blockSize) {
                std::uint16_t recordType;
                psdread<byteOrder>(io, recordType);

                if (recordType == 6) {
                    io.skip(24);
                    dbgFile << "\trecord" << recordType;
                } else if (recordType == 7) {
                    PkRectF bounds;
                    // unsure if there can be multiple clipboard records...
                    bounds.setTop(psdreadFixedPoint<byteOrder>(io));
                    bounds.setLeft(psdreadFixedPoint<byteOrder>(io));
                    bounds.setBottom(psdreadFixedPoint<byteOrder>(io));
                    bounds.setRight(psdreadFixedPoint<byteOrder>(io));
                    vectorMask.path.clipBoardBounds = bounds;
                    vectorMask.path.clipBoardResolution = psdreadFixedPoint<byteOrder>(io);
                    dbgFile << "\trecord" << recordType << "top"
                            << QRectF(bounds.left(), bounds.top(), bounds.width(), bounds.height())
                            << "res" << vectorMask.path.clipBoardResolution;
                    io.skip(4);
                } else if (recordType == 0 || recordType == 3) {
                    std::uint16_t length;
                    psdread<byteOrder>(io, length);
                    dbgFile << "\trecord" << recordType << "length" << length;
                    if (firstPath) {
                        currentPath.isClosed = (recordType == 0);
                    } else {
                        vectorMask.path.subPaths.append(currentPath);
                        currentPath = psd_path_sub_path();
                        currentPath.isClosed = (recordType == 0);
                    }
                    firstPath = false;
                    io.skip(22);
                } else if (recordType == 8) {
                    std::uint16_t length;
                    psdread<byteOrder>(io, length);
                    dbgFile << "\trecord" << recordType << "length" << length;
                    vectorMask.path.initialFillRecord = (length > 0);
                    io.skip(22);
                } else {
                    psd_path_node node;
                    node.control1.setY(psdreadFixedPoint<byteOrder>(io));
                    node.control1.setX(psdreadFixedPoint<byteOrder>(io));
                    node.node.setY(psdreadFixedPoint<byteOrder>(io));
                    node.node.setX(psdreadFixedPoint<byteOrder>(io));
                    node.control2.setY(psdreadFixedPoint<byteOrder>(io));
                    node.control2.setX(psdreadFixedPoint<byteOrder>(io));
                    node.isSmooth = (recordType == 1 || recordType == 4);
                    dbgFile << "\trecord" << recordType << "c1"
                             << QPointF(node.control1.x(), node.control1.y())
                             << "node" << QPointF(node.node.x(), node.node.y())
                             << "c2" << QPointF(node.control2.x(), node.control2.y());
                    currentPath.nodes.append(node);
                }

                currentPos += 26;
            }
            if (!currentPath.nodes.isEmpty()) {
                vectorMask.path.subPaths.append(currentPath);
            }

        } else if (key == "TySh") {
            textData = KisAslReader::readTypeToolObjectSettings(io, textTransform, byteOrder);
        } else if (key == "ffxi") {
        } else if (key == "lnsr") {
        } else if (key == "shpa") {
        } else if (key == "shmd") {
        } else if (key == "lyvr") {
        } else if (key == "tsly") {
        } else if (key == "lmgm") {
        } else if (key == "vmgm") {
        } else if (key == "plLd") { // Replaced by SoLd in CS3

        } else if (key == "linkD" || key == "lnk2" || key == "lnk3") {
        } else if (key == "CgEd") {
        } else if (key == "Txt2") { // global text data, basically the same as an Illustrator text object.
            // Docs say "first 4 are length", this is not true for this particular block, only when in ASL is first 4 length.
            PkByteArray ba;
            ba.resize(static_cast<int>(blockSize));
            const auto bytesRead = io.read(ba.data(), blockSize);
            ba.resize(bytesRead > 0 ? static_cast<int>(bytesRead) : 0);
            KisCosParser p;
            txt2Data = KisTxt2Utils::uncompressKeys(p.parseCosToJson(&ba));
        } else if (key == "pths") {
        } else if (key == "anFX") {
        } else if (key == "FMsk") {
        } else if (key == "SoLd") {
        } else if (key == "vstk") { // vector stroke info
            vectorStroke = KisAslReader::readVectorStroke(io, byteOrder);
        } else if (key == "vscg") {
            if (blockSize > 4) {
                PkString vscgKey = readFixedString<byteOrder>(io);
                fillConfig = KisAslReader::readFillLayer(io, byteOrder);
                if (vscgKey == "SoCo") {
                    fillType = psd_fill_solid_color;
                } else if (vscgKey == "GdFl") {
                    // Gradient Fill
                    fillType = psd_fill_gradient;
                } else if (vscgKey == "PtFl") {
                    // Pattern Fill
                    fillType = psd_fill_pattern;
                }
            }
        } else if (key == "sn2P") {
        } else if (key == "vogk") { // Live path shapes, these are similar to parametric shapes.
            vectorOriginationData = KisAslReader::readVectorOriginationData(io, byteOrder);
        } else if (key == "Mtrn" || key == "Mt16" || key == "Mt32") { // There is no data associated with these keys.

        } else if (key == "LMsk") {
            // TIFFs store the global mask here.
            if (m_header.tiffStyleLayerBlock) {
                int offset = m_header.version > 1 ? 8 : 4;
                dbgFile << "(TIFF) Offset for block handler: " << io.pos() << offset;
                io.seek(io.pos() - offset);
                m_userMaskBlockHandler(io);
            }
        } else if (key == "FXid") {
        } else if (key == "FEid") {
        }
    }
}

bool PsdAdditionalLayerInfoBlock::write(PkStream & /*io*/, KisNodeSP /*node*/)
{
    return true;
}

bool PsdAdditionalLayerInfoBlock::valid()
{
    return true;
}

void PsdAdditionalLayerInfoBlock::writeLuniBlockEx(PkStream &io, const PkString &layerName)
{
    switch (m_header.byteOrder) {
    case psd_byte_order::psdLittleEndian:
        writeLuniBlockExImpl<psd_byte_order::psdLittleEndian>(io, layerName);
        break;
    default:
        writeLuniBlockExImpl(io, layerName);
        break;
    }
}

template<psd_byte_order byteOrder>
void PsdAdditionalLayerInfoBlock::writeLuniBlockExImpl(PkStream &io, const PkString &layerName)
{
    KisAslWriterUtils::writeFixedString<byteOrder>("8BIM", io);
    KisAslWriterUtils::writeFixedString<byteOrder>("luni", io);
    KisAslWriterUtils::OffsetStreamPusher<std::uint32_t, byteOrder> layerNameSizeTag(io, 2);
    KisAslWriterUtils::writeUnicodeString<byteOrder>(layerName, io);
}

void PsdAdditionalLayerInfoBlock::writeLsctBlockEx(PkStream &io, psd_section_type sectionType, bool isPassThrough, const PkString &blendModeKey)
{
    switch (m_header.byteOrder) {
    case psd_byte_order::psdLittleEndian:
        writeLsctBlockExImpl<psd_byte_order::psdLittleEndian>(io, sectionType, isPassThrough, blendModeKey);
        break;
    default:
        writeLsctBlockExImpl(io, sectionType, isPassThrough, blendModeKey);
        break;
    }
}

template<psd_byte_order byteOrder>
void PsdAdditionalLayerInfoBlock::writeLsctBlockExImpl(PkStream &io, psd_section_type sectionType, bool isPassThrough, const PkString &blendModeKey)
{
    KisAslWriterUtils::writeFixedString<byteOrder>("8BIM", io);
    KisAslWriterUtils::writeFixedString<byteOrder>("lsct", io);
    KisAslWriterUtils::OffsetStreamPusher<std::uint32_t, byteOrder> sectionTypeSizeTag(io, 2);
    SAFE_WRITE_EX(byteOrder, io, (std::uint32_t)sectionType);

    PkString realBlendModeKey = isPassThrough ? PkString("pass") : blendModeKey;

    KisAslWriterUtils::writeFixedString<byteOrder>("8BIM", io);
    KisAslWriterUtils::writeFixedString<byteOrder>(realBlendModeKey, io);
}

void PsdAdditionalLayerInfoBlock::writeLfx2BlockEx(PkStream &io, const PkXmlDocument &stylesXmlDoc, bool useLfxsLayerStyleFormat)
{
    switch (m_header.byteOrder) {
    case psd_byte_order::psdLittleEndian:
        writeLfx2BlockExImpl<psd_byte_order::psdLittleEndian>(io, stylesXmlDoc, useLfxsLayerStyleFormat);
        break;
    default:
        writeLfx2BlockExImpl(io, stylesXmlDoc, useLfxsLayerStyleFormat);
        break;
    }
}

template<psd_byte_order byteOrder>
void PsdAdditionalLayerInfoBlock::writeLfx2BlockExImpl(PkStream &io, const PkXmlDocument &stylesXmlDoc, bool useLfxsLayerStyleFormat)
{
    KisAslWriterUtils::writeFixedString<byteOrder>("8BIM", io);
    // 'lfxs' format is used for Group layers in PS
    KisAslWriterUtils::writeFixedString<byteOrder>(!useLfxsLayerStyleFormat ? "lfx2" : "lfxs", io);
    KisAslWriterUtils::OffsetStreamPusher<std::uint32_t, byteOrder> lfx2SizeTag(io, 2);

    try {
        KisAslWriter writer(byteOrder);
        writer.writePsdLfx2SectionEx(io, stylesXmlDoc);

    } catch (KisAslWriterUtils::ASLWriteException &e) {
        warnKrita << "WARNING: Couldn't save layer style lfx2 block:" << PREPEND_METHOD(e.what());

        // TODO: make this error recoverable!
        throw e;
    }
}

void PsdAdditionalLayerInfoBlock::writePattBlockEx(PkStream &io, const PkXmlDocument &patternsXmlDoc)
{
    switch (m_header.byteOrder) {
    case psd_byte_order::psdLittleEndian:
        writePattBlockExImpl<psd_byte_order::psdLittleEndian>(io, patternsXmlDoc);
        break;
    default:
        writePattBlockExImpl(io, patternsXmlDoc);
        break;
    }
}

void PsdAdditionalLayerInfoBlock::writeLclrBlockEx(PkStream &io, const std::uint16_t &labelColor)
{
    switch (m_header.byteOrder) {
    case psd_byte_order::psdLittleEndian:
        writeLclrBlockExImpl<psd_byte_order::psdLittleEndian>(io, labelColor);
        break;
    default:
        writeLclrBlockExImpl(io, labelColor);
        break;
    }
}

void PsdAdditionalLayerInfoBlock::writeFillLayerBlockEx(PkStream &io, const PkXmlDocument &fillConfig, psd_fill_type type)
{
    switch (m_header.byteOrder) {
    case psd_byte_order::psdLittleEndian:
        writeFillLayerBlockExImpl<psd_byte_order::psdLittleEndian>(io, fillConfig, type);
        break;
    default:
        writeFillLayerBlockExImpl(io, fillConfig, type);
        break;
    }
}

void PsdAdditionalLayerInfoBlock::writeVmskBlockEx(PkStream &io, psd_vector_mask mask)
{
    switch (m_header.byteOrder) {
    case psd_byte_order::psdLittleEndian:
        writeVectorMaskImpl<psd_byte_order::psdLittleEndian>(io, mask);
        break;
    default:
        writeVectorMaskImpl(io, mask);
        break;
    }
}

void PsdAdditionalLayerInfoBlock::writeTypeToolBlockEx(PkStream &io, psd_layer_type_shape typeTool)
{
    switch (m_header.byteOrder) {
    case psd_byte_order::psdLittleEndian:
        writeTypeToolImpl<psd_byte_order::psdLittleEndian>(io, typeTool);
        break;
    default:
        writeTypeToolImpl(io, typeTool);
        break;
    }
}

void PsdAdditionalLayerInfoBlock::writeVectorStrokeDataEx(PkStream &io, const PkXmlDocument &vectorStroke)
{
    switch (m_header.byteOrder) {
    case psd_byte_order::psdLittleEndian:
        writeVectorStrokeDataImpl<psd_byte_order::psdLittleEndian>(io, vectorStroke);
        break;
    default:
        writeVectorStrokeDataImpl(io, vectorStroke);
        break;
    }
}

void PsdAdditionalLayerInfoBlock::writeVectorOriginationDataEx(PkStream &io, const PkXmlDocument &vectorOrigination)
{
    switch (m_header.byteOrder) {
    case psd_byte_order::psdLittleEndian:
        writeVectorOriginationDataImpl<psd_byte_order::psdLittleEndian>(io, vectorOrigination);
        break;
    default:
        writeVectorOriginationDataImpl(io, vectorOrigination);
        break;
    }
}

template<psd_byte_order byteOrder>
void PsdAdditionalLayerInfoBlock::writePattBlockExImpl(PkStream &io, const PkXmlDocument &patternsXmlDoc)
{
    KisAslWriterUtils::writeFixedString<byteOrder>("8BIM", io);
    KisAslWriterUtils::writeFixedString<byteOrder>("Patt", io);
    const std::uint32_t padding = m_header.tiffStyleLayerBlock ? 4 : 2;
    KisAslWriterUtils::OffsetStreamPusher<std::uint32_t, byteOrder> pattSizeTag(io, padding);

    try {
        KisAslPatternsWriter writer(patternsXmlDoc, io, byteOrder);
        writer.writePatterns();

    } catch (KisAslWriterUtils::ASLWriteException &e) {
        warnKrita << "WARNING: Couldn't save layer style patterns block:" << PREPEND_METHOD(e.what());

        // TODO: make this error recoverable!
        throw e;
    }
}

template<psd_byte_order byteOrder>
void PsdAdditionalLayerInfoBlock::writeLclrBlockExImpl(PkStream &io, const std::uint16_t &lclr)
{
    KisAslWriterUtils::writeFixedString<byteOrder>("8BIM", io);
    KisAslWriterUtils::writeFixedString<byteOrder>("lclr", io);
    // 4x2 std::uint16_t
    const std::uint32_t len = 8;
    SAFE_WRITE_EX(byteOrder, io, len);
    std::uint16_t zero = 0;
    SAFE_WRITE_EX(byteOrder, io, lclr);
    SAFE_WRITE_EX(byteOrder, io, zero);
    SAFE_WRITE_EX(byteOrder, io, zero);
    SAFE_WRITE_EX(byteOrder, io, zero);


}

template<psd_byte_order byteOrder>
void PsdAdditionalLayerInfoBlock::writeFillLayerBlockExImpl(PkStream &io, const PkXmlDocument &fillConfig, psd_fill_type type)
{
    KisAslWriterUtils::writeFixedString<byteOrder>("8BIM", io);
    if (type == psd_fill_solid_color) {
        KisAslWriterUtils::writeFixedString<byteOrder>("SoCo", io);
    } else if (type == psd_fill_gradient) {
        KisAslWriterUtils::writeFixedString<byteOrder>("GdFl", io);
    } else {
        KisAslWriterUtils::writeFixedString<byteOrder>("PtFl", io);
    }
    KisAslWriterUtils::OffsetStreamPusher<std::uint32_t, byteOrder> fillSizeTag(io, 2);

    try {
        KisAslWriter writer(byteOrder);

        writer.writeFillLayerSectionEx(io, fillConfig);

    } catch (KisAslWriterUtils::ASLWriteException &e) {
        warnKrita << "WARNING: Couldn't save fill layer block:" << PREPEND_METHOD(e.what());

        // TODO: make this error recoverable!
        throw e;
    }
}

template<psd_byte_order byteOrder>
void PsdAdditionalLayerInfoBlock::writeVectorMaskImpl(PkStream &io, psd_vector_mask mask)
{
    KisAslWriterUtils::writeFixedString<byteOrder>("8BIM", io);
    KisAslWriterUtils::writeFixedString<byteOrder>("vmsk", io);

    std::uint32_t len = 8; //, 4 version, 4 flags
    len += 52; // 26 path record, 26 initial fill rule.
    Q_FOREACH(psd_path_sub_path subPath, mask.path.subPaths) {
        len += 26; // subpath record;
        len += subPath.nodes.size() * 26; //and one for each node.
    }

    SAFE_WRITE_EX(byteOrder, io, len);

    std::uint32_t version = 3;
    SAFE_WRITE_EX(byteOrder, io, version);
    std::uint32_t flags = 0;
    if (mask.invert) {
        flags |= 1;
    }
    if (mask.notLink) {
        flags |= 2;
    }
    if (mask.disable) {
        flags |= 4;
    }
    SAFE_WRITE_EX(byteOrder, io, flags);

    // start path records
    std::uint16_t recordType = 6;
    std::uint32_t zero = 0;
    SAFE_WRITE_EX(byteOrder, io, recordType);
    // 24 empty bits
    SAFE_WRITE_EX(byteOrder, io, zero);
    SAFE_WRITE_EX(byteOrder, io, zero);
    SAFE_WRITE_EX(byteOrder, io, zero);
    SAFE_WRITE_EX(byteOrder, io, zero);
    SAFE_WRITE_EX(byteOrder, io, zero);
    SAFE_WRITE_EX(byteOrder, io, zero);
    // initial fill rule record
    recordType = 8;
    SAFE_WRITE_EX(byteOrder, io, recordType);
    std::uint16_t fillType = mask.path.initialFillRecord? 1: 0;
    SAFE_WRITE_EX(byteOrder, io, fillType);
    // 22 empty bits
    const std::uint16_t halfZero = 0;
    SAFE_WRITE_EX(byteOrder, io, halfZero);
    SAFE_WRITE_EX(byteOrder, io, zero);
    SAFE_WRITE_EX(byteOrder, io, zero);
    SAFE_WRITE_EX(byteOrder, io, zero);
    SAFE_WRITE_EX(byteOrder, io, zero);
    SAFE_WRITE_EX(byteOrder, io, zero);
    // write the subpaths.
    Q_FOREACH(psd_path_sub_path subPath, mask.path.subPaths) {
        recordType = subPath.isClosed? 0: 3;
        std::uint16_t length = subPath.nodes.size();
        dbgFile << "writing subpath" << subPath.nodes.size();
        SAFE_WRITE_EX(byteOrder, io, recordType);
        SAFE_WRITE_EX(byteOrder, io, length);
        // 22 empty bits
        SAFE_WRITE_EX(byteOrder, io, halfZero);
        SAFE_WRITE_EX(byteOrder, io, zero);
        SAFE_WRITE_EX(byteOrder, io, zero);
        SAFE_WRITE_EX(byteOrder, io, zero);
        SAFE_WRITE_EX(byteOrder, io, zero);
        SAFE_WRITE_EX(byteOrder, io, zero);

        Q_FOREACH(psd_path_node node, subPath.nodes) {
            if (subPath.isClosed) {
                recordType = node.isSmooth? 1: 2;
            } else {
                recordType = node.isSmooth? 4: 5;
            }
            SAFE_WRITE_EX(byteOrder, io, recordType);
            psdwriteFixedPoint<byteOrder>(io, node.control1.y());
            psdwriteFixedPoint<byteOrder>(io, node.control1.x());
            psdwriteFixedPoint<byteOrder>(io, node.node.y());
            psdwriteFixedPoint<byteOrder>(io, node.node.x());
            psdwriteFixedPoint<byteOrder>(io, node.control2.y());
            psdwriteFixedPoint<byteOrder>(io, node.control2.x());
        }
    }
}

template<psd_byte_order byteOrder>
void PsdAdditionalLayerInfoBlock::writeTypeToolImpl(PkStream &io, psd_layer_type_shape tool)
{
    KisAslWriterUtils::writeFixedString<byteOrder>("8BIM", io);
    KisAslWriterUtils::writeFixedString<byteOrder>("TySh", io);

    KisAslWriterUtils::OffsetStreamPusher<std::uint32_t, byteOrder> tyshSizeTag(io, 2);

    try {
        KisAslWriter writer(byteOrder);

        writer.writeTypeToolObjectSettings(io, tool.textDataASLXML(), tool.textWarpXML(), tool.transform, tool.boundingBox);

    } catch (KisAslWriterUtils::ASLWriteException &e) {
        warnKrita << "WARNING: Couldn't save text layer block:" << PREPEND_METHOD(e.what());

        // TODO: make this error recoverable!
        throw e;
    }
}

template<psd_byte_order byteOrder>
void PsdAdditionalLayerInfoBlock::writeVectorStrokeDataImpl(PkStream &io, const PkXmlDocument &vectorStroke)
{
    KisAslWriterUtils::writeFixedString<byteOrder>("8BIM", io);
    KisAslWriterUtils::writeFixedString<byteOrder>("vstk", io);
    KisAslWriterUtils::OffsetStreamPusher<std::uint32_t, byteOrder> strokeSizeTag(io, 2);
    try {
        KisAslWriter writer(byteOrder);

        writer.writeVectorStrokeDataEx(io, vectorStroke);

    } catch (KisAslWriterUtils::ASLWriteException &e) {
        warnKrita << "WARNING: Couldn't save vector stroke layer block:" << PREPEND_METHOD(e.what());

        // TODO: make this error recoverable!
        throw e;
    }
}

void PsdAdditionalLayerInfoBlock::writeTxt2BlockEx(PkStream &io, const PkVariantHash txt2Hash)
{
    switch (m_header.byteOrder) {
    case psd_byte_order::psdLittleEndian:
        writeTxt2BlockExImpl<psd_byte_order::psdLittleEndian>(io, txt2Hash);
        break;
    default:
        writeTxt2BlockExImpl(io, txt2Hash);
        break;
    }
}

template<psd_byte_order byteOrder>
void PsdAdditionalLayerInfoBlock::writeTxt2BlockExImpl(PkStream &io, const PkVariantHash txt2Hash)
{
    KisAslWriterUtils::writeFixedString<byteOrder>("8BIM", io);
    KisAslWriterUtils::writeFixedString<byteOrder>("Txt2", io);

    PkByteArray ba = KisCosWriter::writeTxt2FromVariantHash(txt2Hash);
    std::uint32_t length = ba.size();
    SAFE_WRITE_EX(byteOrder, io, length);
    io.write(ba.constData(), ba.size());
}

template<psd_byte_order byteOrder>
void PsdAdditionalLayerInfoBlock::writeVectorOriginationDataImpl(PkStream &io, const PkXmlDocument &vectorOrigination)
{
    KisAslWriterUtils::writeFixedString<byteOrder>("8BIM", io);
    KisAslWriterUtils::writeFixedString<byteOrder>("vogk", io);
    KisAslWriterUtils::OffsetStreamPusher<std::uint32_t, byteOrder> strokeSizeTag(io, 2);
    try {
        KisAslWriter writer(byteOrder);

        writer.writeVectorOriginationDataEx(io, vectorOrigination);

    } catch (KisAslWriterUtils::ASLWriteException &e) {
        warnKrita << "WARNING: Couldn't save vector stroke layer block:" << PREPEND_METHOD(e.what());

        // TODO: make this error recoverable!
        throw e;
    }
}
