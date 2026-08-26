/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_colorize_dom_utils.h"

#include <KoColorSpaceRegistry.h>
#include "kis_dom_utils.h"
#include "lazybrush/kis_lazy_fill_tools.h"
#include "kis_kra_tags.h"
#include "kis_paint_device.h"

namespace KisDomUtils {
    void saveValue(PkXmlElement *parent, const PkString &tag, const KisLazyFillTools::KeyStroke &stroke)
    {
        using namespace KRA;

        PkXmlDocument doc = parent->ownerDocument();
        PkXmlElement e = doc.createElement(tag);
        parent->appendChild(e);

        e.setAttribute("type", COLORIZE_KEYSTROKE);

        PkString fileName = tag;
        fileName.replace("item", COLORIZE_KEYSTROKE);

        e.setAttribute(FILE_NAME, fileName);
        e.setAttribute(COLORIZE_KEYSTROKE_IS_TRANSPARENT, stroke.isTransparent);

        PkByteArray colorData = PkByteArray::fromRawData((const char*)stroke.color.data(), stroke.color.colorSpace()->pixelSize());
        e.setAttribute(COLORBYTEDATA, PkString(colorData.toBase64()));
    }

    bool loadValue(const PkXmlElement &e, KisLazyFillTools::KeyStroke *stroke, const KoColorSpace *colorSpace, const PkPoint &offset)
    {
        using namespace KRA;

        if (!Private::checkType(e, COLORIZE_KEYSTROKE)) return false;

        stroke->isTransparent = toInt(e.attribute(COLORIZE_KEYSTROKE_IS_TRANSPARENT, "0"));

        PkByteArray colorData = PkByteArray::fromBase64(e.attribute(COLORBYTEDATA).toLatin1());
        KoColor color((const quint8*)colorData.data(), colorSpace);
        stroke->color = color;

        stroke->dev = new KisPaintDevice(KoColorSpaceRegistry::instance()->alpha8());
        stroke->dev->moveTo(offset);

        return true;
    }
}
