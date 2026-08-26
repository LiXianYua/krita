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
#include <QFileInfo>
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
    PkString result = filename;
    int pos = filename.lastIndexOf('.');
    result.remove(pos, 4);
    PkTextStream(&result) << "_" << id;
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

        if (!s1.compare(name)) {
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
    // GAP: PkString 内部是 vector<char16_t>（UTF-16），但公开 API 无 fromUtf16/
    // PkFromUtf16 构造入口（仅 PkFromUtf8/const char*）。ABR 名是 UTF-16 码元。
    // 建议 pk 库补：static PkString PkFromUtf16(const char16_t* s, int len)（实现约 3 行）。
    // 本行保持 migrate 产物不动，待 Task 6/7 compat 桩或 pk 侧补 API 后接上。
    PkString name_utf8 = PkString::fromUtf16(name_ucs2, buf_size);
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
            PkMemoryStream buf;
            buf.open(PkFileStream::ReadWrite);
            brushTipImage.save(&buf, "PNG");
            abrBrush->setMD5Sum(KoMD5Generator::generateHash(buf.data()));
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
        if (name.isNull()) {
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
                PkMemoryStream buf;
                buf.open(PkFileStream::ReadWrite);
                brushTipImage.save(&buf, "PNG");
                abrBrush->setMD5Sum(KoMD5Generator::generateHash(buf.data()));
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

bool KisAbrBrushCollection::load()
{
    m_isLoaded = true;
    PkFileStream file(filename());
    QFileInfo info(file);
    m_lastModified = info.lastModified();
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
    PkMemoryStream buf(&ba);
    buf.open(PkStream::ReadOnly);
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
        layer_ID = abr_brush_load(abr, &abr_hdr, QFileInfo(filename()).fileName(), image_ID, i + 1);
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
