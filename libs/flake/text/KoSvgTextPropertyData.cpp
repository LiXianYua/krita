/*
 *  SPDX-FileCopyrightText: 2024 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <QtCore/QtCore>
#include <PkFlakeBridge.h>
#include "KoSvgTextPropertyData.h"
#include <pk/log/PkDebug.h>

namespace {

// PkDebug 不流式输出容器，这里把 map/set 拼成可读字符串。
// MapT 泛化：过渡头 convertParagraphProperties/convertToSvgTextAttributes 返回真 Qt
// QMap<QString,QString>（real-Qt-first TU 里 QString 是真 Qt），key()/value() 是真 QString，
// 跨界用 toPkString 显式转 PkString。
template <typename MapT>
PkString mapToString(const MapT &m)
{
    PkString out;
    for (auto it = m.begin(); it != m.end(); ++it) {
        if (!out.isEmpty()) {
            out += PkString(", ");
        }
        out += toPkString(it.key());
        out += PkString("=");
        out += toPkString(it.value());
    }
    return out;
}

PkString setToString(const PkSet<KoSvgTextProperties::PropertyId> &s)
{
    PkString out;
    for (auto v : s) {
        if (!out.isEmpty()) {
            out += PkString(", ");
        }
        out += PkString(std::to_string(static_cast<int>(v)).c_str());
    }
    return out;
}

}

PkDebug operator<<(PkDebug dbg, const KoSvgTextPropertyData &prop)
{
    dbg.nospace() << "TextPropertyData(";
    dbg.nospace() << " Common properties:" << mapToString(prop.commonProperties.convertParagraphProperties()) << mapToString(prop.commonProperties.convertToSvgTextAttributes());
    dbg.nospace() << " Tristate:" << setToString(prop.tristate);
    dbg.nospace() << " SpanSelection:" << prop.spanSelection;
    dbg.nospace() << " Enabled:" << prop.enabled;
    dbg.nospace() << " )";
    return dbg.space();
}
