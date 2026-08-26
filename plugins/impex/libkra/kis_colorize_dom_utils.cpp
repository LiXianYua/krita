/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_colorize_dom_utils.h"

#include <cstdint>
#include <string>
#include <vector>

#include <KoColorSpaceRegistry.h>
#include "kis_dom_utils.h"
#include "lazybrush/kis_lazy_fill_tools.h"
#include "kis_kra_tags.h"
#include "kis_paint_device.h"

namespace {

// QByteArray::toBase64 / fromBase64 的零 Qt 对应（复制自 libs/global/KoProperties.cpp
// 的 pkBase64Encode/Decode——那里是文件局部 helper，无公开头；此处照抄保持独立）。
const char kB64Alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string pkBase64Encode(const PkByteArray &data)
{
    std::string out;
    const char *src = data.constData();
    const int len = data.size();
    for (int i = 0; i < len; i += 3) {
        const unsigned int n =
            (static_cast<unsigned char>(src[i]) << 16) |
            (i + 1 < len ? static_cast<unsigned char>(src[i + 1]) << 8 : 0u) |
            (i + 2 < len ? static_cast<unsigned char>(src[i + 2]) : 0u);
        out.push_back(kB64Alphabet[(n >> 18) & 0x3f]);
        out.push_back(kB64Alphabet[(n >> 12) & 0x3f]);
        out.push_back(i + 1 < len ? kB64Alphabet[(n >> 6) & 0x3f] : '=');
        out.push_back(i + 2 < len ? kB64Alphabet[n & 0x3f] : '=');
    }
    return out;
}

int pkBase64Value(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

PkByteArray pkBase64Decode(const std::string &s)
{
    std::vector<unsigned char> out;
    unsigned int buf = 0;
    int bits = 0;
    for (char c : s) {
        if (c == '=') break;
        const int v = pkBase64Value(c);
        if (v < 0) break;
        buf = (buf << 6) | static_cast<unsigned int>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<unsigned char>((buf >> bits) & 0xff));
        }
    }
    return PkByteArray(reinterpret_cast<const char *>(out.data()),
                       static_cast<int>(out.size()));
}

} // namespace

namespace KisDomUtils {
    void saveValue(PkXmlElement *parent, const PkString &tag, const KisLazyFillTools::KeyStroke &stroke)
    {
        using namespace KRA;

        PkXmlDocument doc = parent->ownerDocument();
        PkXmlElement e = doc.createElement(tag);
        parent->appendChild(e);

        e.setAttribute("type", COLORIZE_KEYSTROKE);

        // 对拍原 Qt：QString::replace("item", COLORIZE_KEYSTROKE) 全量替换。
        // PkString 无 replace，用 std::string::replace 中转。
        std::string fileNameUtf8 = tag.PkToUtf8();
        const std::string from("item");
        const std::string to = COLORIZE_KEYSTROKE.PkToUtf8();
        std::size_t pos = 0;
        while ((pos = fileNameUtf8.find(from, pos)) != std::string::npos) {
            fileNameUtf8.replace(pos, from.size(), to);
            pos += to.size();
        }
        e.setAttribute(FILE_NAME, PkString::PkFromUtf8(fileNameUtf8.c_str(), static_cast<int>(fileNameUtf8.size())));

        e.setAttribute(COLORIZE_KEYSTROKE_IS_TRANSPARENT, PkString(stroke.isTransparent ? "true" : "false"));

        // 对拍原 Qt：QByteArray::fromRawData(ptr, len) → QByteArray(ptr, len)；
        // toBase64 → 本地 pkBase64Encode。
        PkByteArray colorData((const char*)stroke.color.data(), stroke.color.colorSpace()->pixelSize());
        const std::string b64 = pkBase64Encode(colorData);
        e.setAttribute(COLORBYTEDATA, PkString::PkFromUtf8(b64.c_str(), static_cast<int>(b64.size())));
    }

    bool loadValue(const PkXmlElement &e, KisLazyFillTools::KeyStroke *stroke, const KoColorSpace *colorSpace, const PkPoint &offset)
    {
        using namespace KRA;

        if (!Private::checkType(e, COLORIZE_KEYSTROKE)) return false;

        stroke->isTransparent = toInt(e.attribute(COLORIZE_KEYSTROKE_IS_TRANSPARENT, "0"));

        // 对拍原 Qt：QByteArray::fromBase64(attr.toLatin1()) → 本地 pkBase64Decode。
        // PkString 无 toLatin1，Base64 是 ASCII，PkToUtf8() 等价。
        PkByteArray colorData = pkBase64Decode(e.attribute(COLORBYTEDATA).PkToUtf8());
        KoColor color((const uint8_t*)colorData.data(), colorSpace);
        stroke->color = color;

        stroke->dev = new KisPaintDevice(KoColorSpaceRegistry::instance()->alpha8());
        stroke->dev->moveTo(offset);

        return true;
    }
}
