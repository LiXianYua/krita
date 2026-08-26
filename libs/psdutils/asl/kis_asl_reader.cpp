/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_asl_reader.h"

#include "kis_dom_utils.h"

#include <cstring>
#include <stdexcept>
#include <string>

#include <PkXmlDocument.h>
#include <PkStream.h>
#include <PkCosMemoryStream.h>
#include <PkRgb.h>

#include "compression.h"
#include "kis_asl_byte_utils.h"
#include "kis_offset_on_exit_verifier.h"
#include "psd.h"
#include "psd_utils.h"

#include "kis_asl_reader_utils.h"
#include "kis_asl_writer_utils.h"


namespace Private {
// PkByteArray 无 append()/operator[]，RLE 逐行累积与按索引取字节在这里补齐。
inline void pkAppendBytes(PkByteArray &dst, const PkByteArray &src)
{
    const int oldSize = dst.size();
    const int addSize = src.size();
    dst.resize(oldSize + addSize);
    if (addSize > 0) {
        std::memcpy(dst.data() + oldSize, src.constData(), static_cast<size_t>(addSize));
    }
}
}

namespace Private
{
/**
 * Numerical fetch functions
 *
 * We read numbers and convert them to strings to be able to store
 * them in XML.
 */

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
PkString readDoubleAsString(PkStream &device)
{
    double value = 0.0;
    SAFE_READ_EX(byteOrder, device, value);

    return KisDomUtils::toString(value);
}

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
PkString readIntAsString(PkStream &device)
{
    quint32 value = 0.0;
    SAFE_READ_EX(byteOrder, device, value);

    return KisDomUtils::toString(value);
}

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
PkString readBoolAsString(PkStream &device)
{
    quint8 value = 0.0;
    SAFE_READ_EX(byteOrder, device, value);

    return KisDomUtils::toString(value);
}

/**
 * XML generation functions
 *
 * Add a node and fill the corresponding attributes
 */

PkXmlElement appendXMLNodeCommon(const PkString &key, const PkString &value, const PkString &type, PkXmlElement *parent, PkXmlDocument *doc)
{
    PkXmlElement el = doc->createElement("node");
    if (!key.isEmpty()) {
        el.setAttribute("key", key);
    }
    el.setAttribute("type", type);
    el.setAttribute("value", value);
    parent->appendChild(el);

    return el;
}

PkXmlElement appendXMLNodeCommonNoValue(const PkString &key, const PkString &type, PkXmlElement *parent, PkXmlDocument *doc)
{
    PkXmlElement el = doc->createElement("node");
    if (!key.isEmpty()) {
        el.setAttribute("key", key);
    }
    el.setAttribute("type", type);
    parent->appendChild(el);

    return el;
}

void appendIntegerXMLNode(const PkString &key, const PkString &value, PkXmlElement *parent, PkXmlDocument *doc)
{
    appendXMLNodeCommon(key, value, "Integer", parent, doc);
}

void appendDoubleXMLNode(const PkString &key, const PkString &value, PkXmlElement *parent, PkXmlDocument *doc)
{
    appendXMLNodeCommon(key, value, "Double", parent, doc);
}

void appendTextXMLNode(const PkString &key, const PkString &value, PkXmlElement *parent, PkXmlDocument *doc)
{
    appendXMLNodeCommon(key, value, "Text", parent, doc);
}

void appendPointXMLNode(const PkString &key, const PkPointF &pt, PkXmlElement *parent, PkXmlDocument *doc)
{
    PkXmlElement el = appendXMLNodeCommonNoValue(key, "Descriptor", parent, doc);
    el.setAttribute("classId", "CrPt");
    el.setAttribute("name", "");

    appendDoubleXMLNode("Hrzn", KisDomUtils::toString(pt.x()), &el, doc);
    appendDoubleXMLNode("Vrtc", KisDomUtils::toString(pt.x()), &el, doc);
}

/**
 * ASL -> XML parsing functions
 */

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
void readDescriptor(PkStream &device, const PkString &key, PkXmlElement *parent, PkXmlDocument *doc);

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
void readChildObject(PkStream &device, PkXmlElement *parent, PkXmlDocument *doc, bool skipKey = false)
{
    using namespace KisAslReaderUtils;

    PkString key;

    if (!skipKey) {
        key = readVarString<byteOrder>(device);
    }

    PkString OSType = readFixedString<byteOrder>(device);

    // dbgKrita << "Child" << ppVar(key) << ppVar(OSType);

    if (OSType == "obj ") {
        throw KisAslReaderUtils::ASLParseException("OSType 'obj' not implemented");

    } else if (OSType == "Objc" || OSType == "GlbO") {
        readDescriptor<byteOrder>(device, key, parent, doc);

    } else if (OSType == "VlLs") {
        quint32 numItems = GARBAGE_VALUE_MARK;
        SAFE_READ_EX(byteOrder, device, numItems);

        PkXmlElement el = appendXMLNodeCommonNoValue(key, "List", parent, doc);
        for (quint32 i = 0; i < numItems; i++) {
            readChildObject<byteOrder>(device, &el, doc, true);
        }

    } else if (OSType == "doub") {
        appendDoubleXMLNode(key, readDoubleAsString<byteOrder>(device), parent, doc);

    } else if (OSType == "UntF") {
        const PkString unit = readFixedString<byteOrder>(device);
        const PkString value = readDoubleAsString<byteOrder>(device);

        PkXmlElement el = appendXMLNodeCommon(key, value, "UnitFloat", parent, doc);
        el.setAttribute("unit", unit);

    } else if (OSType == "TEXT") {
        PkString unicodeString = readUnicodeString<byteOrder>(device);
        appendTextXMLNode(key, unicodeString, parent, doc);

    } else if (OSType == "enum") {
        const PkString typeId = readVarString<byteOrder>(device);
        const PkString value = readVarString<byteOrder>(device);

        PkXmlElement el = appendXMLNodeCommon(key, value, "Enum", parent, doc);
        el.setAttribute("typeId", typeId);

    } else if (OSType == "long") {
        appendIntegerXMLNode(key, readIntAsString<byteOrder>(device), parent, doc);

    } else if (OSType == "bool") {
        const PkString value = readBoolAsString<byteOrder>(device);
        appendXMLNodeCommon(key, value, "Boolean", parent, doc);

    } else if (OSType == "type") {
        throw KisAslReaderUtils::ASLParseException("OSType 'type' not implemented");
    } else if (OSType == "GlbC") {
        throw KisAslReaderUtils::ASLParseException("OSType 'GlbC' not implemented");
    } else if (OSType == "alis") {
        throw KisAslReaderUtils::ASLParseException("OSType 'alis' not implemented");
    } else if (OSType == "tdta") {

        if (key == "EngineData") {
            // This is PSD text engine data, which outputs
            // Carousel Object Structure (PDF) data, much like the "Txt2" additional info block.
            quint32 len = 0.0;
            SAFE_READ_EX(byteOrder, device, len);
            PkByteArray ba;
            ba.resize(len);
            const PkStream::pk_int64 nRead = device.read(ba.data(), len);
            ba.resize(static_cast<int>(nRead));

            PkXmlCDATASection dataSection;
            dataSection = doc->createCDATASection(pkToBase64(ba));
            PkXmlElement dataElement = doc->createElement("node");
            dataElement.setAttribute("type", "RawData");
            dataElement.setAttribute("key", key);
            dataElement.appendChild(dataSection);
            parent->appendChild(dataElement);
        } else {
            throw KisAslReaderUtils::ASLParseException("OSType 'tdta' not implemented");
        }
    }
}



template<psd_byte_order byteOrder>
void readDescriptor(PkStream &device, const PkString &key, PkXmlElement *parent, PkXmlDocument *doc)
{
    using namespace KisAslReaderUtils;

    PkString name = readUnicodeString(device);
    PkString classId = readVarString(device);

    quint32 numChildren = GARBAGE_VALUE_MARK;
    SAFE_READ_EX(byteOrder, device, numChildren);

    PkXmlElement el = appendXMLNodeCommonNoValue(key, "Descriptor", parent, doc);
    el.setAttribute("classId", classId);
    el.setAttribute("name", name);

    // dbgKrita << "Descriptor" << ppVar(key) << ppVar(classId) << ppVar(numChildren);

    for (quint32 i = 0; i < numChildren; i++) {
        readChildObject<byteOrder>(device, &el, doc);
    }
}

template<psd_byte_order byteOrder>
PkImage readVirtualArrayList(PkStream &device, int numPlanes, const PkVector<PkRgb> &palette)
{
    using namespace KisAslReaderUtils;

    quint32 arrayVersion = GARBAGE_VALUE_MARK;
    SAFE_READ_EX(byteOrder, device, arrayVersion);

    if (arrayVersion != 3) {
        throw ASLParseException("VAList version is not '3'!");
    }

    quint32 arrayLength = GARBAGE_VALUE_MARK;
    SAFE_READ_EX(byteOrder, device, arrayLength);

    SETUP_OFFSET_VERIFIER(vaEndVerifier, device, arrayLength, 100);

    quint32 x0 = 0;
    quint32 y0 = 0;
    quint32 x1 = 0;
    quint32 y1 = 0;
    SAFE_READ_EX(byteOrder, device, y0);
    SAFE_READ_EX(byteOrder, device, x0);
    SAFE_READ_EX(byteOrder, device, y1);
    SAFE_READ_EX(byteOrder, device, x1);
    PkRect arrayRect(x0, y0, x1 - x0, y1 - y0);

    quint32 numberOfChannels = GARBAGE_VALUE_MARK;
    SAFE_READ_EX(byteOrder, device, numberOfChannels);

    if (numberOfChannels != 24) {
        throw ASLParseException("VAList: Krita doesn't support ASL files with 'numberOfChannels' flag not equal to 24 (it is not documented)!");
    }

    dbgKrita << ppVar(arrayVersion);
    dbgKrita << ppVar(arrayLength);
    dbgKrita << ppVar(arrayRect);
    dbgKrita << ppVar(numberOfChannels);

    if (numPlanes != 1 && numPlanes != 3) {
        throw ASLParseException("VAList: unsupported number of planes!");
    }

    PkVector<PkByteArray> dataPlanes;
    dataPlanes.resize(3);

    quint32 pixelDepth1 = GARBAGE_VALUE_MARK;

    for (int i = 0; i < numPlanes; i++) {
        quint32 arrayWritten = GARBAGE_VALUE_MARK;
        if (!psdread<byteOrder>(device, arrayWritten) || !arrayWritten) {
            throw ASLParseException("VAList plane has not-written flag set!");
        }

        quint32 arrayPlaneLength = GARBAGE_VALUE_MARK;
        if (!psdread<byteOrder>(device, arrayPlaneLength) || !arrayPlaneLength) {
            throw ASLParseException("VAList has plane length set to zero!");
        }

        SETUP_OFFSET_VERIFIER(planeEndVerifier, device, arrayPlaneLength, 0);
        qint64 nextPos = device.pos() + arrayPlaneLength;

        SAFE_READ_EX(byteOrder, device, pixelDepth1);

        quint32 x0 = 0;
        quint32 y0 = 0;
        quint32 x1 = 0;
        quint32 y1 = 0;
        SAFE_READ_EX(byteOrder, device, y0);
        SAFE_READ_EX(byteOrder, device, x0);
        SAFE_READ_EX(byteOrder, device, y1);
        SAFE_READ_EX(byteOrder, device, x1);
        PkRect planeRect(x0, y0, x1 - x0, y1 - y0);

        if (planeRect != arrayRect) {
            throw ASLParseException("VAList: planes are not uniform. Not supported yet!");
        }

        quint16 pixelDepth2 = GARBAGE_VALUE_MARK;
        SAFE_READ_EX(byteOrder, device, pixelDepth2);

        quint8 useCompression = 9;
        SAFE_READ_EX(byteOrder, device, useCompression);

        // dbgKrita << "plane index:" << ppVar(i);
        // dbgKrita << ppVar(arrayWritten);
        // dbgKrita << ppVar(arrayPlaneLength);
        // dbgKrita << ppVar(pixelDepth1);
        // dbgKrita << ppVar(planeRect);
        // dbgKrita << ppVar(pixelDepth2);
        // dbgKrita << ppVar(useCompression);

        if (pixelDepth1 != pixelDepth2) {
            throw ASLParseException("VAList: two pixel depths of the plane are not equal (it is not documented)!");
        }

        if (pixelDepth1 != 1 && pixelDepth1 != 8 && pixelDepth1 != 16) {
            throw ASLParseException(PkString("VAList: unsupported pixel depth: %1!").arg(static_cast<int>(pixelDepth1)));
        }

        const int channelSize = (pixelDepth1 == 1 || pixelDepth1 == 8) ? 1 : 2;

        const int dataLength = planeRect.width() * planeRect.height() * channelSize;

        if (useCompression == psd_compression_type::Uncompressed) {
            dataPlanes[i].resize(arrayPlaneLength - 23);
            const PkStream::pk_int64 nRead = device.read(dataPlanes[i].data(), arrayPlaneLength - 23);
            dataPlanes[i].resize(static_cast<int>(nRead));
        } else if (useCompression == psd_compression_type::RLE) {
            const int numRows = planeRect.height();

            PkVector<quint16> rowSizes;
            rowSizes.resize(numRows);

            for (int row = 0; row < numRows; row++) {
                quint16 rowSize = GARBAGE_VALUE_MARK;
                SAFE_READ_EX(byteOrder, device, rowSize);
                rowSizes[row] = rowSize;
            }

            for (int row = 0; row < numRows; row++) {
                const quint16 rowSize = rowSizes[row];

                PkByteArray compressedData;
                compressedData.resize(rowSize);
                const PkStream::pk_int64 nRead = device.read(compressedData.data(), rowSize);
                compressedData.resize(static_cast<int>(nRead));

                if (compressedData.size() != rowSize) {
                    throw ASLParseException("VAList: failed to read compressed data!");
                }

                dbgFile << "Going to decompress the pattern";

                PkByteArray uncompressedData =
                    Compression::uncompress(planeRect.width() * channelSize, compressedData, psd_compression_type::RLE);

                if (uncompressedData.size() != planeRect.width()) {
                    throw ASLParseException("VAList: failed to decompress data!");
                }

                Private::pkAppendBytes(dataPlanes[i], uncompressedData);
            }
        } else if (useCompression == psd_compression_type::ZIP) {
            PkByteArray compressedBytes;
            compressedBytes.resize(arrayPlaneLength - 23);
            const PkStream::pk_int64 nRead = device.read(compressedBytes.data(), arrayPlaneLength - 23);
            compressedBytes.resize(static_cast<int>(nRead));
            dataPlanes[i] = Compression::uncompress(dataLength, compressedBytes, psd_compression_type::ZIP);
        } else {
            throw ASLParseException("VAList: ZIP compression is not implemented yet!");
        }

        if (dataPlanes[i].size() != dataLength) {
            throw ASLParseException("VAList: failed to read/uncompress data plane!");
        }

        if (device.pos() != nextPos) {
            warnFile << "VAList: Data is left out from reading"
                     << "(" << device.pos() << ")";
        }
        device.seek(nextPos);
    }

    PkImage::Format format{};
    
    if (pixelDepth1 == 1 || !palette.isEmpty()) {
        if (palette.isEmpty()) {
            format = PkImage::Format_Grayscale8;
        } else {
            format = PkImage::Format_Indexed8;
        }
    } else if (pixelDepth1 == 8) {
        format = PkImage::Format_ARGB32;
    } else {
        format = PkImage::Format_RGBA64;
    }

    PkImage image(arrayRect.size(), format);

    if (format == PkImage::Format_Indexed8) {
        // PkImage::setColorTable 收 std::vector<uint32_t>，palette 是 PkVector<PkRgb>，
        // 用迭代器区间构造转换。
        image.setColorTable(std::vector<uint32_t>(palette.begin(), palette.end()));
    }
    dbgFile << "Loading the data into an image of format" << format << arrayRect << "(" << device.pos() << ")";

    const int dataLength = arrayRect.width() * arrayRect.height();

    if (format == PkImage::Format_ARGB32) {
        quint8 *dstPtr = image.bits();

        // This copies the single channel data into all three rgb channels, creating a grayscale picture
        for (int i = 0; i < dataLength; i++) {
            for (int j = 2; j >= 0; j--) {
                int plane;
                if (numPlanes == 1) {
                    plane = 0;
                }
                else {
                    plane = j;
                }
                *dstPtr++ = dataPlanes[plane].constData()[i];
            }
            *dstPtr++ = 0xFF;
        }
    } else if (format == PkImage::Format_Indexed8 || format == PkImage::Format_Grayscale8) {
        const auto *dataPlane = reinterpret_cast<const quint8 *>(dataPlanes[0].constData());

        for (int x = 0; x < arrayRect.height(); x++) {
            quint8 *dstPtr = image.scanLine(x);

            for (int y = 0; y < arrayRect.width(); y++) {
                *dstPtr++ = dataPlane[x * arrayRect.width() + y];
            }
        }
    } else {
        quint16 *dstPtr = reinterpret_cast<quint16 *>(image.bits());

        for (int i = 0; i < dataLength; i++) {
            for (int j = 0; j <= 2; j++) {
                const int plane = qMin(numPlanes, j);
                const quint16 *dataPlane = reinterpret_cast<const quint16 *>(dataPlanes[plane].constData());
                *dstPtr++ = psdFromBigEndian<quint16>(dataPlane[i]);
            }
            *dstPtr++ = 0xFFFF;
        }
    }

    // static int i = -1; i++;
    // PkString filename = PkString("pattern_image_%1.png").arg(i);
    // dbgKrita << "### dumping pattern image" << ppVar(filename);
    // image.save(filename);

    return image.convertToFormat(PkImage::Format_ARGB32);
}

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
qint64 readPattern(PkStream &device, PkXmlElement *parent, PkXmlDocument *doc)
{
    using namespace KisAslReaderUtils;

    quint32 patternSize = GARBAGE_VALUE_MARK;
    SAFE_READ_EX(byteOrder, device, patternSize);

    // patterns are always aligned by 4 bytes
    patternSize = KisAslWriterUtils::alignOffsetCeil(patternSize, 4);

    SETUP_OFFSET_VERIFIER(patternEndVerifier, device, patternSize, 0);

    quint32 patternVersion = GARBAGE_VALUE_MARK;
    SAFE_READ_EX(byteOrder, device, patternVersion);

    if (patternVersion != 1) {
        throw ASLParseException("Pattern version is not \'1\'");
    }

    quint32 patternImageMode = GARBAGE_VALUE_MARK;
    SAFE_READ_EX(byteOrder, device, patternImageMode);

    dbgFile << "Pattern format:" << patternImageMode << "(" << device.pos() << ")";

    quint16 patternHeight = GARBAGE_VALUE_MARK;
    SAFE_READ_EX(byteOrder, device, patternHeight);

    dbgFile << "Pattern height:" << patternHeight << "(" << device.pos() << ")";

    quint16 patternWidth = GARBAGE_VALUE_MARK;
    SAFE_READ_EX(byteOrder, device, patternWidth);

    dbgFile << "Pattern width:" << patternHeight << "(" << device.pos() << ")";

    PkString patternName;
    psdread_unicodestring<byteOrder>(device, patternName);

    dbgFile << "Pattern name:" << patternName << "(" << device.pos() << ")";

    PkString patternUuid = readPascalString<byteOrder>(device);

    dbgFile << "Pattern UUID:" << patternUuid << "(" << device.pos() << ")";

    // dbgKrita << "--";
    // dbgKrita << ppVar(patternSize);
    // dbgKrita << ppVar(patternImageMode);
    // dbgKrita << ppVar(patternHeight);
    // dbgKrita << ppVar(patternWidth);
    // dbgKrita << ppVar(patternName);
    // dbgKrita << ppVar(patternUuid);

    int numPlanes = 0;
    psd_color_mode mode = static_cast<psd_color_mode>(patternImageMode);

    switch (mode) {
    case MultiChannel:
    case Grayscale:
    case Indexed:
        numPlanes = 1;
        break;
    case RGB:
        numPlanes = 3;
        break;
    default: {
        PkString msg = PkString("Unsupported image mode: %1!").arg(mode);
        throw ASLParseException(msg);
    }
    }

    PkVector<PkRgb> palette;

    if (mode == Indexed) {

        palette.resize(256);

        for(auto i = 0; i < 256; i++) {
            quint8 r = 0;
            quint8 g = 0;
            quint8 b = 0;
            psdread<byteOrder>(device, r);
            psdread<byteOrder>(device, g);
            psdread<byteOrder>(device, b);
            palette[i] = pkRgb(r, g, b);
        }

        dbgFile << "Palette: " << palette << "(" << device.pos() << ")";

        // XXX: there's no way to detect this. Assume the 772 length
        quint16 validColours = GARBAGE_VALUE_MARK;
        psdread<byteOrder>(device, validColours);
        palette.resize(validColours);
        dbgFile << "Palette real size:" << validColours << "(" << device.pos() << ")";

        // Set transparent colour
        quint16 transparentIdx = GARBAGE_VALUE_MARK;
        psdread<byteOrder>(device, transparentIdx);
        dbgFile << "Transparent index:" << transparentIdx << "(" << device.pos() << ")";

        palette[transparentIdx] = pkRgba(pkRed(palette[transparentIdx]),
                                         pkGreen(palette[transparentIdx]),
                                         pkBlue(palette[transparentIdx]), 0x00);
    }

    /**
     * Create XML data
     */

    PkXmlElement pat = doc->createElement("node");

    pat.setAttribute("classId", "KisPattern");
    pat.setAttribute("type", "Descriptor");
    pat.setAttribute("name", "");

    PkByteArray patBytes;
    PkCosMemoryStream patternBuf(&patBytes);
    patternBuf.open(PkStream::WriteOnly);

    { // ensure we don't keep resources for too long
        // XXX: this PkImage should tolerate 16-bit and higher
        PkString fileName = PkString("%1.pat").arg(patternUuid);
        PkImage patternImage = readVirtualArrayList<byteOrder>(device, numPlanes, palette);
        KoPattern realPattern(patternImage, patternName, fileName);
        realPattern.savePatToDevice(&patternBuf);
    }

    /**
     * We are loading the pattern and convert it into ARGB right away,
     * so we need not store real image mode and size of the pattern
     * externally.
     */
    appendTextXMLNode("Nm  ", patternName, &pat, doc);
    appendTextXMLNode("Idnt", patternUuid, &pat, doc);

    PkXmlCDATASection dataSection = doc->createCDATASection(pkToBase64(pkQCompress(patBytes)));

    PkXmlElement dataElement = doc->createElement("node");
    dataElement.setAttribute("type", "KisPatternData");
    dataElement.setAttribute("key", "Data");

    dataElement.appendChild(dataSection);
    pat.appendChild(dataElement);
    parent->appendChild(pat);

    return sizeof(patternSize) + patternSize;
}

PkXmlDocument readFileImpl(PkStream &device)
{
    using namespace KisAslReaderUtils;

    PkXmlDocument doc;
    PkXmlElement root = doc.createElement("asl");
    doc.appendChild(root);

    {
        quint16 stylesVersion = GARBAGE_VALUE_MARK;
        SAFE_READ_SIGNATURE_EX(psd_byte_order::psdBigEndian, device, stylesVersion, 2);
    }

    {
        quint32 aslSignature = GARBAGE_VALUE_MARK;
        const quint32 refSignature = 0x3842534c; // '8BSL' in little-endian
        SAFE_READ_SIGNATURE_EX(psd_byte_order::psdBigEndian, device, aslSignature, refSignature);
    }

    {
        quint16 patternsVersion = GARBAGE_VALUE_MARK;
        SAFE_READ_SIGNATURE_EX(psd_byte_order::psdBigEndian, device, patternsVersion, 3);
    }

    // Patterns

    {
        quint32 patternsSize = GARBAGE_VALUE_MARK;
        SAFE_READ_EX(psd_byte_order::psdBigEndian, device, patternsSize);

        if (patternsSize > 0) {
            SETUP_OFFSET_VERIFIER(patternsSectionVerifier, device, patternsSize, 0);

            PkXmlElement patternsRoot = doc.createElement("node");
            patternsRoot.setAttribute("type", "List");
            patternsRoot.setAttribute("key", ResourceType::Patterns);
            root.appendChild(patternsRoot);

            try {
                qint64 bytesRead = 0;
                while (bytesRead < patternsSize) {
                    qint64 chunk = readPattern(device, &patternsRoot, &doc);
                    bytesRead += chunk;
                }
            } catch (ASLParseException &e) {
                warnKrita << "WARNING: ASL (emb. pattern):" << e.what();
            }
        }
    }

    // Styles

    quint32 numStyles = GARBAGE_VALUE_MARK;
    SAFE_READ_EX(psd_byte_order::psdBigEndian, device, numStyles);

    for (int i = 0; i < (int)numStyles; i++) {
        quint32 bytesToRead = GARBAGE_VALUE_MARK;
        SAFE_READ_EX(psd_byte_order::psdBigEndian, device, bytesToRead);

        SETUP_OFFSET_VERIFIER(singleStyleSectionVerifier, device, bytesToRead, 0);

        {
            quint32 stylesFormatVersion = GARBAGE_VALUE_MARK;
            SAFE_READ_SIGNATURE_EX(psd_byte_order::psdBigEndian, device, stylesFormatVersion, 16);
        }

        readDescriptor(device, "", &root, &doc);

        {
            quint32 stylesFormatVersion = GARBAGE_VALUE_MARK;
            SAFE_READ_SIGNATURE_EX(psd_byte_order::psdBigEndian, device, stylesFormatVersion, 16);
        }

        readDescriptor(device, "", &root, &doc);
    }

    return doc;
}

} // namespace Private

PkXmlDocument KisAslReader::readFile(PkStream &device)
{
    PkXmlDocument doc;

    if (device.isSequential()) {
        warnKrita << "WARNING: *** KisAslReader::readFile: the supplied"
                  << "IO device is sequential. Chances are that"
                  << "the layer style will *not* be loaded correctly!";
    }

    try {
        doc = Private::readFileImpl(device);
    } catch (KisAslReaderUtils::ASLParseException &e) {
        warnKrita << "WARNING: ASL:" << e.what();
    }

    return doc;
}

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
PkXmlDocument readLfx2PsdSectionImpl(PkStream &device);

PkXmlDocument KisAslReader::readLfx2PsdSection(PkStream &device, psd_byte_order byteOrder)
{
    switch (byteOrder) {
    case psd_byte_order::psdLittleEndian:
        return readLfx2PsdSectionImpl<psd_byte_order::psdLittleEndian>(device);
    default:
        return readLfx2PsdSectionImpl(device);
    }
}

template<psd_byte_order byteOrder>
PkXmlDocument readLfx2PsdSectionImpl(PkStream &device)
{
    PkXmlDocument doc;

    if (device.isSequential()) {
        warnKrita << "WARNING: *** KisAslReader::readLfx2PsdSection: the supplied"
                  << "IO device is sequential. Chances are that"
                  << "the layer style will *not* be loaded correctly!";
    }

    try {
        {
            quint32 objectEffectsVersion = GARBAGE_VALUE_MARK;
            const quint32 ref = 0x00;
            SAFE_READ_SIGNATURE_EX(byteOrder, device, objectEffectsVersion, ref);
        }

        {
            quint32 descriptorVersion = GARBAGE_VALUE_MARK;
            const quint32 ref = 0x10;
            SAFE_READ_SIGNATURE_EX(byteOrder, device, descriptorVersion, ref);
        }

        PkXmlElement root = doc.createElement("asl");
        doc.appendChild(root);

        Private::readDescriptor<byteOrder>(device, "", &root, &doc);

    } catch (KisAslReaderUtils::ASLParseException &e) {
        warnKrita << "WARNING: PSD: lfx2 section:" << e.what();
    }

    return doc;
}

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
PkXmlDocument readFillLayerImpl(PkStream &device);

PkXmlDocument KisAslReader::readFillLayer(PkStream &device, psd_byte_order byteOrder)
{
    switch (byteOrder) {
    case psd_byte_order::psdLittleEndian:
        return readFillLayerImpl<psd_byte_order::psdLittleEndian>(device);
    default:
        return readFillLayerImpl(device);
    }
}

template<psd_byte_order byteOrder>
PkXmlDocument readFillLayerImpl(PkStream &device)
{
    PkXmlDocument doc;

    if (device.isSequential()) {
        warnKrita << "WARNING: *** KisAslReader::readFillLayerPsdSection: the supplied"
                  << "IO device is sequential. Chances are that"
                  << "the fill config will *not* be loaded correctly!";
    }
    try {

        {
            quint32 descriptorVersion = GARBAGE_VALUE_MARK;
            SAFE_READ_SIGNATURE_EX(byteOrder, device, descriptorVersion, 16);
        }

        PkXmlElement root = doc.createElement("asl");
        doc.appendChild(root);

        Private::readDescriptor<byteOrder>(device, "", &root, &doc);

    } catch (KisAslReaderUtils::ASLParseException &e) {
        warnKrita << "WARNING: PSD: SoCo section:" << e.what();
    }

    return doc;
}

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
PkXmlDocument readTypeToolObjectSettingsImpl(PkStream &device, PkTransform &transform);

PkXmlDocument KisAslReader::readTypeToolObjectSettings(PkStream &device, PkTransform &transform, psd_byte_order byteOrder)
{
    switch (byteOrder) {
    case psd_byte_order::psdLittleEndian:
        return readTypeToolObjectSettingsImpl<psd_byte_order::psdLittleEndian>(device, transform);
    default:
        return readTypeToolObjectSettingsImpl(device, transform);
    }
}

template<psd_byte_order byteOrder>
PkXmlDocument readTypeToolObjectSettingsImpl(PkStream &device, PkTransform &transform)
{
    PkXmlDocument doc;

    if (device.isSequential()) {
        warnKrita << "WARNING: *** KisAslReader::readTypeToolObjectSettings: the supplied"
                  << "IO device is sequential. Chances are that"
                  << "the fill config will *not* be loaded correctly!";
    }
    try {

        {
            quint16 descriptorVersion = GARBAGE_VALUE_MARK;
            SAFE_READ_SIGNATURE_EX(byteOrder, device, descriptorVersion, 1);

        }

        // transform matrix.
        double xx = GARBAGE_VALUE_MARK;
        SAFE_READ_EX(byteOrder, device, xx);
        double xy = GARBAGE_VALUE_MARK;
        SAFE_READ_EX(byteOrder, device, xy);
        double yx = GARBAGE_VALUE_MARK;
        SAFE_READ_EX(byteOrder, device, yx);
        double yy = GARBAGE_VALUE_MARK;
        SAFE_READ_EX(byteOrder, device, yy);
        double tx = GARBAGE_VALUE_MARK;
        SAFE_READ_EX(byteOrder, device, tx);
        double ty = GARBAGE_VALUE_MARK;
        SAFE_READ_EX(byteOrder, device, ty);

        {
            quint16 textVersion = GARBAGE_VALUE_MARK;
            SAFE_READ_SIGNATURE_EX(byteOrder, device, textVersion, 50);
            quint32 descriptorVersion = GARBAGE_VALUE_MARK;
            SAFE_READ_SIGNATURE_EX(byteOrder, device, descriptorVersion, 16);
        }

        transform = PkTransform(xx, xy, yx, yy, tx, ty);

        PkXmlElement root = doc.createElement("asl");
        doc.appendChild(root);
        // text layer data
        Private::readDescriptor<byteOrder>(device, "", &root, &doc);

        // warp data
        {
            quint16 textVersion = GARBAGE_VALUE_MARK;
            SAFE_READ_SIGNATURE_EX(byteOrder, device, textVersion, 1);
            quint32 descriptorVersion = GARBAGE_VALUE_MARK;
            SAFE_READ_SIGNATURE_EX(byteOrder, device, descriptorVersion, 16);
        }

        Private::readDescriptor<byteOrder>(device, "", &root, &doc);

        /*// bounding box
        quint64 left = GARBAGE_VALUE_MARK;
        SAFE_READ_EX(byteOrder, device, left);
        quint64 top = GARBAGE_VALUE_MARK;
        SAFE_READ_EX(byteOrder, device, top);
        quint64 right = GARBAGE_VALUE_MARK;
        SAFE_READ_EX(byteOrder, device, right);
        quint64 bottom = GARBAGE_VALUE_MARK;
        SAFE_READ_EX(byteOrder, device, bottom);
        */


    } catch (KisAslReaderUtils::ASLParseException &e) {
        warnKrita << "WARNING: PSD: TySh section:" << e.what();
    }

    return doc;
}

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
PkXmlDocument readVectorStrokeImpl(PkStream &device);

PkXmlDocument KisAslReader::readVectorStroke(PkStream &device, psd_byte_order byteOrder)
{
    switch (byteOrder) {
    case psd_byte_order::psdLittleEndian:
        return readVectorStrokeImpl<psd_byte_order::psdLittleEndian>(device);
    default:
        return readVectorStrokeImpl(device);
    }
}

template<psd_byte_order byteOrder>
PkXmlDocument readVectorStrokeImpl(PkStream &device)
{
    PkXmlDocument doc;

    if (device.isSequential()) {
        warnKrita << "WARNING: *** KisAslReader::readVectorStroke: the supplied"
                  << "IO device is sequential. Chances are that"
                  << "the fill config will *not* be loaded correctly!";
    }
    try {

        {
            quint32 descriptorVersion = GARBAGE_VALUE_MARK;
            SAFE_READ_SIGNATURE_EX(byteOrder, device, descriptorVersion, 16);
        }

        PkXmlElement root = doc.createElement("asl");
        doc.appendChild(root);

        Private::readDescriptor<byteOrder>(device, "", &root, &doc);

    } catch (KisAslReaderUtils::ASLParseException &e) {
        warnKrita << "WARNING: PSD: vmsk section:" << e.what();
    }

    return doc;
}

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
PkXmlDocument readVectorOriginationDataImpl(PkStream &device);
PkXmlDocument KisAslReader::readVectorOriginationData(PkStream &device, psd_byte_order byteOrder)
{
    switch (byteOrder) {
    case psd_byte_order::psdLittleEndian:
        return readVectorOriginationDataImpl<psd_byte_order::psdLittleEndian>(device);
    default:
        return readVectorOriginationDataImpl(device);
    }
}

template<psd_byte_order byteOrder>
PkXmlDocument readVectorOriginationDataImpl(PkStream &device)
{
    PkXmlDocument doc;

    if (device.isSequential()) {
        warnKrita << "WARNING: *** KisAslReader::readVectorStroke: the supplied"
                  << "IO device is sequential. Chances are that"
                  << "the fill config will *not* be loaded correctly!";
    }
    try {

        {
            quint32 version = GARBAGE_VALUE_MARK;
            SAFE_READ_SIGNATURE_EX(byteOrder, device, version, 1);
        }
        {
            quint32 descriptorVersion = GARBAGE_VALUE_MARK;
            SAFE_READ_SIGNATURE_EX(byteOrder, device, descriptorVersion, 16);
        }

        PkXmlElement root = doc.createElement("asl");
        doc.appendChild(root);

        Private::readDescriptor<byteOrder>(device, "", &root, &doc);

    } catch (KisAslReaderUtils::ASLParseException &e) {
        warnKrita << "WARNING: PSD: vogk section:" << e.what();
    }

    return doc;
}

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
PkXmlDocument readPsdSectionPatternImpl(PkStream &device, qint64 bytesLeft);

PkXmlDocument KisAslReader::readPsdSectionPattern(PkStream &device, qint64 bytesLeft, psd_byte_order byteOrder)
{
    switch (byteOrder) {
    case psd_byte_order::psdLittleEndian:
        return readPsdSectionPatternImpl<psd_byte_order::psdLittleEndian>(device, bytesLeft);
    default:
        return readPsdSectionPatternImpl(device, bytesLeft);
    }
}

template<psd_byte_order byteOrder>
PkXmlDocument readPsdSectionPatternImpl(PkStream &device, qint64 bytesLeft)
{
    PkXmlDocument doc;

    PkXmlElement root = doc.createElement("asl");
    doc.appendChild(root);

    PkXmlElement pat = doc.createElement("node");
    root.appendChild(pat);

    pat.setAttribute("classId", ResourceType::Patterns);
    pat.setAttribute("type", "Descriptor");
    pat.setAttribute("name", "");

    try {
        qint64 bytesRead = 0;
        while (bytesRead < bytesLeft) {
            qint64 chunk = Private::readPattern<byteOrder>(device, &pat, &doc);
            bytesRead += chunk;
        }
    } catch (KisAslReaderUtils::ASLParseException &e) {
        warnKrita << "WARNING: PSD (emb. pattern):" << e.what();
    }

    return doc;
}
