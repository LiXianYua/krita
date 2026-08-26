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
#include "kis_kra_utils.h"
#include "kis_paint_device.h"


namespace KisDomUtils {
    void saveValue(PkXmlElement *parent, const PkString &tag, const KisLazyFillTools::KeyStroke &stroke)
    {
        using namespace KRA;

        PkXmlDocument doc = parent->ownerDocument();
        PkXmlElement e = doc.createElement(tag);
        parent->appendChild(e);

        e.setAttribute("type", COLORIZE_KEYSTROKE);

        // 对拍原 Qt：字符串 replace("item", COLORIZE_KEYSTROKE) 全量替换。
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

        e.setAttribute(COLORIZE_KEYSTROKE_IS_TRANSPARENT, PkString(stroke.isTransparent ? "1" : "0"));

        // 对拍原 Qt：fromRawData(ptr, len) → 直接构造(ptr, len)；
        // toBase64 → 本地 pkBase64Encode。
        PkByteArray colorData((const char*)stroke.color.data(), stroke.color.colorSpace()->pixelSize());
        const std::string b64 = KRA::base64Encode(colorData);
        e.setAttribute(COLORBYTEDATA, PkString::PkFromUtf8(b64.c_str(), static_cast<int>(b64.size())));
    }

    bool loadValue(const PkXmlElement &e, KisLazyFillTools::KeyStroke *stroke, const KoColorSpace *colorSpace, const PkPoint &offset)
    {
        using namespace KRA;

        if (!Private::checkType(e, COLORIZE_KEYSTROKE)) return false;

        stroke->isTransparent = toInt(e.attribute(COLORIZE_KEYSTROKE_IS_TRANSPARENT, "0"));

        // 对拍原 Qt：fromBase64(attr.toLatin1()) → 本地 pkBase64Decode。
        // PkString 无 toLatin1，Base64 是 ASCII，PkToUtf8() 等价。
        PkByteArray colorData = KRA::base64Decode(e.attribute(COLORBYTEDATA).PkToUtf8());
        KoColor color((const uint8_t*)colorData.data(), colorSpace);
        stroke->color = color;

        stroke->dev = new KisPaintDevice(KoColorSpaceRegistry::instance()->alpha8());
        stroke->dev->moveTo(offset);

        return true;
    }
}
