/*
 *  SPDX-FileCopyrightText: 2010 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2010 Lukáš Tvrdý <lukast.dev@gmail.com>
 *  SPDX-FileCopyrightText: 2007 Eric Lamarque <eric.lamarque@free.fr>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <QtEndian>

#include "kis_abr_brush_collection.h"
#include "kis_abr_brush.h"

#include <PkXmlElement.h>
#include <PkFileStream.h>
#include <PkImage.h>
#include <PkPoint.h>
#include <PkColor.h>
#include <PkAuxTypes.h>
#include <kis_debug.h>
#include <PkString.h>
#include <PkMemoryStream.h>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <system_error>
#include <KoMD5Generator.h>
#include <klocalizedstring.h>

#include <KoColor.h>


struct AbrInfo {
    //big endian
    short version;
    short subversion;
    // count of the images (brushes) in the abr file
    short count;
};

/// save the QImages as png files to directory image_tests
static PkImage convertToQImage(char * buffer, qint32 width, qint32 height)
{
    // create 8-bit indexed image
    PkImage img(width, height, PkImage::Format_RGB32);
    int pos = 0;
    int value = 0;
    for (int y = 0; y < height; y++) {
        PkRgb *pixel = reinterpret_cast<PkRgb *>(img.scanLine(y));
        for (int x = 0; x < width; x++, pos++) {
            value = 255 - buffer[pos];
            pixel[x] = qRgb(value, value , value);
        }

    }

    return img;
}

static qint32 rle_decode(PkDataStream & abr, char *buffer, qint32 height)
{
    qint32 n;
    char ptmp;
    char ch;
    int i, j, c;
    short *cscanline_len;
    char *data = buffer;

    // read compressed size foreach scanline
    cscanline_len = new short[ height ];
    for (i = 0; i < height; i++) {
        // short
        abr >> cscanline_len[i];
    }

    // unpack each scanline data
    for (i = 0; i < height; i++) {
        for (j = 0; j < cscanline_len[i];) {
            // char
            if (!abr.device()->getChar(&ptmp)) {
                break;
            }
            n = ptmp;

            j++;
            if (n >= 128)     // force sign
                n -= 256;
            if (n < 0) {      // copy the following char -n + 1 times
                if (n == -128)  // it's a nop
                    continue;
                n = -n + 1;
                // char
                if (!abr.device()->getChar(&ch)) {
                    break;
                }

                j++;
                for (c = 0; c < n; c++, data++) {
                    *data = ch;
                }
            }
            else {
                // read the following n + 1 chars (no compr)
                for (c = 0; c < n + 1; c++, j++, data++) {
                    // char
                    if (!abr.device()->getChar(data))  {
                        break;
                    }
                }
            }
        }
    }
    delete [] cscanline_len;
    return 0;
}


static PkString abr_v1_brush_name(const PkString filename, qint32 id)
{
    // 原 Qt 实现：lastIndexOf('.') 定位扩展名起点，remove(pos, 4) 剥掉 ".abr"，
    // QTextStream 追加 "_<id>"。PkString 无 lastIndexOf/remove，且 PkTextStream
    // 只接受 PkStream* 不接受 &PkString，故用 PkString 既有 API 复刻语义：
    // 取最后一个 '.' 之前的部分 + "_<id>"。
    int pos = -1;
    for (int i = filename.size() - 1; i >= 0; --i) {
        if (filename.at(i) == u'.') {
            pos = i;
            break;
        }
    }
    PkString result = (pos >= 0) ? filename.left(pos) : filename;
    result.append(PkString("_%1").arg(id));
    return result;
}

static bool abr_supported_content(AbrInfo *abr_hdr)
{
    switch (abr_hdr->version) {
    case 1:
    case 2:
        return true;
        break;
    case 6:
        if (abr_hdr->subversion == 1 || abr_hdr->subversion == 2)
            return true;
        break;
    }
    return false;
}

static bool abr_reach_8BIM_section(PkDataStream & abr, const PkString name)
{
    char tag[4];
    char tagname[5];
    qint32 section_size = 0;
    int r;

    // find 8BIMname section
    while (!abr.atEnd()) {
        r = abr.readRawData(tag, 4);

        if (r != 4) {
            warnKrita << "Error: Cannot read 8BIM tag ";
            return false;
        }

        if (strncmp(tag, "8BIM", 4)) {
            warnKrita << "Error: Start tag not 8BIM but " << (int)tag[0] << (int)tag[1] << (int)tag[2] << (int)tag[3] << " at position " << abr.device()->pos();
            return false;
        }

        r = abr.readRawData(tagname, 4);

        if (r != 4) {
            warnKrita << "Error: Cannot read 8BIM tag name";
            return false;
        }
        tagname[4] = '\0';

        // ABR 节名是 4 字节 ASCII 码（"Brushes" 等），PkFromUtf8 逐字节精确；原 Qt fromLatin1 亦按字节映射。
        PkString s1 = PkString::PkFromUtf8(tagname, 4);

        if (s1 == name) {
            return true;
        }

        // long
        abr >> section_size;
        abr.device()->seek(abr.device()->pos() + section_size);
    }
    return true;
}

static qint32 find_sample_count_v6(PkDataStream & abr, AbrInfo *abr_info)
{
    qint64 origin;
    qint32 sample_section_size;
    qint32 sample_section_end;
    qint32 samples = 0;
    qint32 data_start;

    qint32 brush_size;
    qint32 brush_end;

    if (!abr_supported_content(abr_info))
        return 0;

    origin = abr.device()->pos();

    if (!abr_reach_8BIM_section(abr, "samp")) {
        // reset to origin
        abr.device()->seek(origin);
        return 0;
    }

    // long
    abr >> sample_section_size;
    sample_section_end = sample_section_size + abr.device()->pos();

    if(sample_section_end < 0 || sample_section_end > abr.device()->size())
        return 0;

    data_start = abr.device()->pos();

    while ((!abr.atEnd()) && (abr.device()->pos() < sample_section_end)) {
        // read long
        abr >> brush_size;
        brush_end = brush_size;
        // complement to 4
        while (brush_end % 4 != 0) brush_end++;

        qint64 newPos = abr.device()->pos() + brush_end;
        if(newPos > 0 && newPos < abr.device()->size()) {
            abr.device()->seek(newPos);
        }
        else
            return 0;

        samples++;
    }

    // set stream to samples data
    abr.device()->seek(data_start);

    //dbgKrita <<"samples : "<< samples;
    return samples;
}



static bool abr_read_content(PkDataStream & abr, AbrInfo *abr_hdr)
{

    abr >> abr_hdr->version;
    abr_hdr->subversion = 0;
    abr_hdr->count = 0;

    switch (abr_hdr->version) {
    case 1:
    case 2:
        abr >> abr_hdr->count;
        break;
    case 6:
        abr >> abr_hdr->subversion;
        abr_hdr->count = find_sample_count_v6(abr, abr_hdr);
        break;
    default:
        // unknown versions
        break;
    }
    // next bytes in abr are samples data

    return true;
}


// ABR 笔刷名是 UTF-16 码元（ABR 6+ 可含代理对）。PkString 公开 API 无 fromUtf16/
// PkFromUtf16 构造入口（R 线锁，不越权改 pk/string），这里在 libs/brush 内做
// UTF-16→UTF-8 transcode，再经 PkString::PkFromUtf8 构造。代理对（U+10000+）正确
// 合并；孤立高/低代理位替换为 U+FFFD。
static PkString abr_ucs2_to_utf8(const char16_t *ucs2, int len)
{
    if (len <= 0) {
        return PkString();
    }
    std::string utf8;
    utf8.reserve(static_cast<std::size_t>(len));
    for (int i = 0; i < len; ++i) {
        std::uint32_t cp = static_cast<std::uint32_t>(ucs2[i]);
        if (cp >= 0xD800u && cp <= 0xDBFFu) {
            // 高代理位：必须后随低代理位，否则为孤立高代理 → U+FFFD
            if (i + 1 < len) {
                const std::uint32_t lo = static_cast<std::uint32_t>(ucs2[i + 1]);
                if (lo >= 0xDC00u && lo <= 0xDFFFu) {
                    cp = 0x10000u + ((cp - 0xD800u) << 10) + (lo - 0xDC00u);
                    ++i;
                } else {
                    cp = 0xFFFDu;
                }
            } else {
                cp = 0xFFFDu;
            }
        } else if (cp >= 0xDC00u && cp <= 0xDFFFu) {
            // 孤立低代理位 → U+FFFD
            cp = 0xFFFDu;
        }
        if (cp < 0x80u) {
            utf8.push_back(static_cast<char>(cp));
        } else if (cp < 0x800u) {
            utf8.push_back(static_cast<char>(0xC0u | (cp >> 6)));
            utf8.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        } else if (cp < 0x10000u) {
            utf8.push_back(static_cast<char>(0xE0u | (cp >> 12)));
            utf8.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
            utf8.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        } else {
            utf8.push_back(static_cast<char>(0xF0u | (cp >> 18)));
            utf8.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
            utf8.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
            utf8.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        }
    }
    return PkString::PkFromUtf8(utf8.data(), static_cast<int>(utf8.size()));
}

static PkString abr_read_ucs2_text(PkDataStream & abr)
{
    quint32 name_size;
    quint32 buf_size;
    uint   i;
    /* two-bytes characters encoded (UCS-2)
    *  format:
    *   long : size - number of characters in string
    *   data : zero terminated UCS-2 string
    */

    // long
    abr >> name_size;
    if (name_size == 0) {
        return PkString();
    }

    //buf_size = name_size * 2;
    buf_size = name_size;

    //name_ucs2 = (char*) malloc (buf_size * sizeof (char));
    //name_ucs2 = new char[buf_size];

    ushort * name_ucs2 = new ushort[buf_size];
    for (i = 0; i < buf_size ; i++) {
        //* char*/
        //abr >> name_ucs2[i];

        // I will use ushort as that is input to fromUtf16
        abr >>  name_ucs2[i];
    }
    PkString name_utf8 = abr_ucs2_to_utf8(reinterpret_cast<const char16_t *>(name_ucs2),
                                          static_cast<int>(buf_size));
    delete [] name_ucs2;

    return name_utf8;
}


quint32 KisAbrBrushCollection::abr_brush_load_v6(PkDataStream & abr, AbrInfo *abr_hdr, const PkString filename, qint32 image_ID, qint32 id)
{
    Q_UNUSED(image_ID);
    qint32 brush_size = 0;
    qint32 brush_end = 0;
    qint32 next_brush = 0;

    qint32 top, left, bottom, right;
    top = left = bottom = right = 0;
    short depth;
    char compression;

    qint32 width = 0;
    qint32 height = 0;
    qint32 size = 0;

    qint32 layer_ID = -1;

    char *buffer;

    abr >> brush_size;
    brush_end = brush_size;
    // complement to 4
    while (brush_end % 4 != 0) {
        brush_end++;
    }

    next_brush = abr.device()->pos() + brush_end;

    // discard key
    abr.device()->seek(abr.device()->pos() + 37);
    if (abr_hdr->subversion == 1)
        // discard short coordinates and unknown short
        abr.device()->seek(abr.device()->pos() + 10);
    else
        // discard unknown bytes
        abr.device()->seek(abr.device()->pos() + 264);

    // long
    abr >> top;
    abr >> left;
    abr >> bottom;
    abr >> right;
    // short
    abr >> depth;
    // char
    abr.device()->getChar(&compression);

    width = right - left;
    height = bottom - top;
    size = width * (depth >> 3) * height;

    // remove .abr and add some id, so something like test.abr -> test_12345
    PkString name = abr_v1_brush_name(filename, id);

    buffer = (char*)malloc(size);

    // data decoding
    if (!compression) {
        // not compressed - read raw bytes as brush data
        //fread (buffer, size, 1, abr);
        abr.readRawData(buffer, size);
    } else {
        rle_decode(abr, buffer, height);
    }

    if (width < quint16_MAX && height < quint16_MAX) {
        // filename - filename of the file , e.g. test.abr
        // name - test_number_of_the_brush, e.g test_1, test_2
        KisAbrBrushSP abrBrush;
        PkImage brushTipImage = convertToQImage(buffer, width, height);
        if (m_abrBrushes->contains(name)) {
            abrBrush = m_abrBrushes.data()->operator[](name);
        }
        else {
            abrBrush = KisAbrBrushSP(new KisAbrBrush(name, this));
            // GAP: PkImage::save 未交付（R-15/S-03-e PNG 编解码通道），笔刷位图
            // 无法编码为 PNG 计算规范 MD5。沿用 png_brush saveToDevice 的桩化
            // 先例，MD5 留空 —— 资源指纹只影响去重/身份，不影响 ABR 加载。
            abrBrush->setMD5Sum(PkString());
        }

        abrBrush->setBrushTipImage(brushTipImage);
        // XXX: call extra setters on abrBrush for other options of ABR brushes
        abrBrush->setValid(true);
        abrBrush->setName(name);
        m_abrBrushes.data()->operator[](name) = abrBrush;

    }

    free(buffer);
    abr.device()->seek(next_brush);

    layer_ID = id;
    return layer_ID;
}


qint32 KisAbrBrushCollection::abr_brush_load_v12(PkDataStream & abr, AbrInfo *abr_hdr, const PkString filename, qint32 image_ID, qint32 id)
{
    Q_UNUSED(image_ID);
    short brush_type;
    qint32 brush_size;
    qint32 next_brush;

    qint32 top, left, bottom, right;
    qint16 depth;
    char compression;
    PkString name;

    qint32 width, height;
    qint32 size;

    qint32 layer_ID = -1;
    char   *buffer;

    // short
    abr >> brush_type;
    // long
    abr >> brush_size;
    next_brush = abr.device()->pos() + brush_size;

    if (brush_type == 1) {
        // computed brush
        // FIXME: support it!
        warnKrita  << "WARNING: computed brush unsupported, skipping.";
        abr.device()->seek(abr.device()->pos() + next_brush);
        // TODO: test also this one abr.skipRawData(next_brush);
    }
    else if (brush_type == 2) {
        // sampled brush
        // discard 4 misc bytes and 2 spacing bytes
        abr.device()->seek(abr.device()->pos() + 6);

        if (abr_hdr->version == 2)
            name = abr_read_ucs2_text(abr);
        if (name.isEmpty()) {
            name = abr_v1_brush_name(filename, id);
        }

        // discard 1 byte for antialiasing and 4 x short for short bounds
        abr.device()->seek(abr.device()->pos() + 9);

        // long
        abr >> top;
        abr >> left;
        abr >> bottom;
        abr >> right;
        // short
        abr >> depth;
        // char
        abr.device()->getChar(&compression);

        width = right - left;
        height = bottom - top;
        size = width * (depth >> 3) * height;

        /* FIXME: support wide brushes */
        if (height > 16384) {
            warnKrita << "WARNING: wide brushes not supported";
            abr.device()->seek(next_brush);
        }
        else {
            buffer = (char*)malloc(size);

            if (!compression) {
                // not compressed - read raw bytes as brush data
                abr.readRawData(buffer, size);
            } else {
                rle_decode(abr, buffer, height);
            }

            KisAbrBrushSP abrBrush;
            PkImage brushTipImage = convertToQImage(buffer, width, height);
            if (m_abrBrushes->contains(name)) {
                abrBrush = m_abrBrushes.data()->operator[](name);
            }
            else {
                abrBrush = KisAbrBrushSP(new KisAbrBrush(name, this));
                // GAP: PkImage::save 未交付（同上一处），MD5 留空。
                abrBrush->setMD5Sum(PkString());
            }

            abrBrush->setBrushTipImage(brushTipImage);
            // XXX: call extra setters on abrBrush for other options of ABR brushes   free (buffer);
            abrBrush->setValid(true);
            abrBrush->setName(name);
            m_abrBrushes.data()->operator[](name) = abrBrush;
            layer_ID = 1;
        }
    }
    else {
        warnKrita << "Unknown ABR brush type, skipping.";
        abr.device()->seek(next_brush);
    }

    return layer_ID;
}


qint32 KisAbrBrushCollection::abr_brush_load(PkDataStream & abr, AbrInfo *abr_hdr, const PkString filename, qint32 image_ID, qint32 id)
{
    qint32 layer_ID = -1;
    switch (abr_hdr->version) {
    case 1:
        Q_FALLTHROUGH();
        // fall through, version 1 and 2 are compatible
    case 2:
        layer_ID = abr_brush_load_v12(abr, abr_hdr, filename, image_ID, id);
        break;
    case 6:
        layer_ID = abr_brush_load_v6(abr, abr_hdr, filename, image_ID, id);
        break;
    }

    return layer_ID;
}


KisAbrBrushCollection::KisAbrBrushCollection(const PkString& filename)
    : m_isLoaded(false)
    , m_lastModified()
    , m_filename(filename)
    , m_abrBrushes(new PkMap<PkString, KisAbrBrushSP>())
{
}

KisAbrBrushCollection::KisAbrBrushCollection(const KisAbrBrushCollection& rhs)
    : m_isLoaded(rhs.m_isLoaded)
    , m_lastModified(rhs.m_lastModified)
{
    m_abrBrushes.reset(new PkMap<PkString, KisAbrBrushSP>());
    for (auto it = rhs.m_abrBrushes->begin();
         it != rhs.m_abrBrushes->end();
         ++it) {

        m_abrBrushes->insert(it.key(), KisAbrBrushSP(new KisAbrBrush(*it.value(), this)));
    }
}

// QFileInfo 在 migrate 后无 Pk 等价（QFileInfo 无 PkFileStream/PkString 构造），
// 这里按 S-02-b PkResourceStorageDesktop::lastModifiedMs 的模式用 std::filesystem
// 复刻「PkString 路径 → 文件名 / 最后修改时间」。
static PkString pathFileName(const PkString &path)
{
    const std::string name = std::filesystem::u8path(path.PkToUtf8()).filename().string();
    return PkString::PkFromUtf8(name.c_str(), static_cast<int>(name.size()));
}

static PkDateTime pathLastModified(const PkString &path)
{
    std::error_code ec;
    const std::filesystem::file_time_type writeTime = std::filesystem::last_write_time(path.PkToUtf8(), ec);
    if (ec) {
        return PkDateTime();
    }
    const auto systemTime = std::chrono::time_point_cast<std::chrono::milliseconds>(
        writeTime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
    return PkDateTime::fromMSecsSinceEpoch(systemTime.time_since_epoch().count());
}

bool KisAbrBrushCollection::load()
{
    m_isLoaded = true;
    PkFileStream file(filename());
    m_lastModified = pathLastModified(filename());
    // check if the file is open correctly
    if (!file.open(PkStream::ReadOnly)) {
        warnKrita << "Can't open file " << filename();
        return false;
    }

    bool res = loadFromDevice(&file);
    file.close();

    return res;

}

bool KisAbrBrushCollection::loadFromDevice(PkStream *dev)
{
    AbrInfo abr_hdr;
    qint32 image_ID;
    int i;
    qint32 layer_ID;

    PkByteArray ba = dev->readAll();
    // QBuffer(QByteArray*) 语义：以既有字节为内容的内存设备。PkMemoryStream
    // （libs/store S-01 交付）没有该构造，改成 ReadWrite 写入字节再回卷读取。
    PkMemoryStream buf;
    buf.open(PkStream::ReadWrite);
    buf.write(ba.constData(), ba.size());
    buf.seek(0);
    PkDataStream abr(&buf);


    if (!abr_read_content(abr, &abr_hdr)) {
        warnKrita << "Error: cannot parse ABR file: " << filename();
        return false;
    }

    if (!abr_supported_content(&abr_hdr)) {
        warnKrita << "ERROR: unable to decode abr format version " << abr_hdr.version << "(subver " << abr_hdr.subversion << ")";
        return false;
    }

    if (abr_hdr.count == 0) {
        errKrita << "ERROR: no sample brush found in " << filename();
        return false;
    }

    image_ID = 123456;

    for (i = 0; i < abr_hdr.count; i++) {
        layer_ID = abr_brush_load(abr, &abr_hdr, pathFileName(filename()), image_ID, i + 1);
        if (layer_ID == -1) {
            warnKrita << "Warning: problem loading brush #" << i << " in " << filename();
        }
    }

    return true;

}

bool KisAbrBrushCollection::save()
{
    return false;
}

bool KisAbrBrushCollection::saveToDevice(PkStream */*dev*/) const
{
    return false;
}

bool KisAbrBrushCollection::isLoaded() const
{
    return m_isLoaded;
}

PkImage KisAbrBrushCollection::image() const
{
    if (m_abrBrushes->size() > 0) {
        return m_abrBrushes->values().first()->image();
    }
    return PkImage();
}

void KisAbrBrushCollection::toXML(PkXmlDocument& d, PkXmlElement& e) const
{
    Q_UNUSED(d);
    Q_UNUSED(e);
    // Do nothing...
}

PkString KisAbrBrushCollection::defaultFileExtension() const
{
    return PkString(".abr");
}
