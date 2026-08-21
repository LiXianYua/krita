/*
 *  SPDX-FileCopyrightText: 2026 S-08 (R-31)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KoFontFamilyMetadata.h"

#include <PkDataStream.h>
#include <PkXmlDocument.h>

namespace KoFontFamilyMetadata {

// ── built-in 编码 ──────────────────────────────────────────────────────

PkVariantMap buildAxisEntry(const PkString &tag,
                            const PkVariantMap &localizedLabels,
                            double min, double max, double value, double defaultValue,
                            bool variableAxis, bool axisHidden)
{
    PkVariantMap entry;
    entry.emplace(PkString(ENTRY_VERSION), PkVariant(FORMAT_VERSION));
    entry.emplace(PkString(ENTRY_TAG), PkVariant(tag));
    entry.emplace(PkString(ENTRY_LOCALIZED_LABELS), PkVariant(localizedLabels));
    entry.emplace(PkString(ENTRY_MIN), PkVariant(min));
    entry.emplace(PkString(ENTRY_MAX), PkVariant(max));
    entry.emplace(PkString(ENTRY_VALUE), PkVariant(value));
    entry.emplace(PkString(ENTRY_DEFAULT_VALUE), PkVariant(defaultValue));
    entry.emplace(PkString(ENTRY_VARIABLE_AXIS), PkVariant(variableAxis));
    entry.emplace(PkString(ENTRY_AXIS_HIDDEN), PkVariant(axisHidden));
    return entry;
}

PkVariantMap buildStyleEntry(const PkVariantMap &localizedLabels,
                             const PkVariantMap &instanceCoords,
                             bool isItalic, bool isOblique)
{
    PkVariantMap entry;
    entry.emplace(PkString(ENTRY_VERSION), PkVariant(FORMAT_VERSION));
    entry.emplace(PkString(ENTRY_LOCALIZED_LABELS), PkVariant(localizedLabels));
    entry.emplace(PkString(ENTRY_INSTANCE_COORDS), PkVariant(instanceCoords));
    entry.emplace(PkString(ENTRY_IS_ITALIC), PkVariant(isItalic));
    entry.emplace(PkString(ENTRY_IS_OBLIQUE), PkVariant(isOblique));
    return entry;
}

// ── built-in 解码 ──────────────────────────────────────────────────────

std::optional<PkAxisEntry> parseAxisEntry(const PkVariantMap &entry)
{
    const auto itVersion = entry.find(PkString(ENTRY_VERSION));
    if (itVersion == entry.end() || itVersion->second.toInt() != FORMAT_VERSION) {
        return std::nullopt;
    }
    const auto itTag = entry.find(PkString(ENTRY_TAG));
    if (itTag == entry.end()) {
        return std::nullopt;
    }

    PkAxisEntry out;
    out.tag = itTag->second.toString();

    const auto itLabels = entry.find(PkString(ENTRY_LOCALIZED_LABELS));
    if (itLabels != entry.end()) {
        if (itLabels->second.type() != PkVariant::Map) {
            return std::nullopt;
        }
        out.localizedLabels = itLabels->second.toMap();
    }

    const auto getDouble = [&entry](const char *key, double fallback) {
        const auto it = entry.find(PkString(key));
        return it == entry.end() ? fallback : it->second.toDouble();
    };
    out.min = getDouble(ENTRY_MIN, -1);
    out.max = getDouble(ENTRY_MAX, -1);
    out.value = getDouble(ENTRY_VALUE, 0);
    out.defaultValue = getDouble(ENTRY_DEFAULT_VALUE, 0);

    const auto itVariable = entry.find(PkString(ENTRY_VARIABLE_AXIS));
    out.variableAxis = (itVariable != entry.end()) && itVariable->second.toBool();
    const auto itHidden = entry.find(PkString(ENTRY_AXIS_HIDDEN));
    out.axisHidden = (itHidden != entry.end()) && itHidden->second.toBool();

    return out;
}

std::optional<PkStyleEntry> parseStyleEntry(const PkVariantMap &entry)
{
    const auto itVersion = entry.find(PkString(ENTRY_VERSION));
    if (itVersion == entry.end() || itVersion->second.toInt() != FORMAT_VERSION) {
        return std::nullopt;
    }

    PkStyleEntry out;
    const auto itLabels = entry.find(PkString(ENTRY_LOCALIZED_LABELS));
    if (itLabels != entry.end()) {
        if (itLabels->second.type() != PkVariant::Map) {
            return std::nullopt;
        }
        out.localizedLabels = itLabels->second.toMap();
    }
    const auto itCoords = entry.find(PkString(ENTRY_INSTANCE_COORDS));
    if (itCoords != entry.end()) {
        if (itCoords->second.type() != PkVariant::Map) {
            return std::nullopt;
        }
        out.instanceCoords = itCoords->second.toMap();
    }

    const auto itItalic = entry.find(PkString(ENTRY_IS_ITALIC));
    out.isItalic = (itItalic != entry.end()) && itItalic->second.toBool();
    const auto itOblique = entry.find(PkString(ENTRY_IS_OBLIQUE));
    out.isOblique = (itOblique != entry.end()) && itOblique->second.toBool();

    return out;
}

// ── 旧 UserType 迁移 ───────────────────────────────────────────────────

namespace {

// Qt QVariant wire（PkDataStream 探针实测，见 probe_legacy_wire.cpp）：
//   容器帧：[qint32 typeId][quint8 nullFlag][quint32 count] + 条目
//   UserType 值：[qint32 typeId>=1024 (Qt5) 或 ==127 (Qt4)][quint8 nullFlag]
//                [QByteArray typeName][payload = QDataStream<<QString(xml)]
struct ValueHeader {
    std::int32_t typeId = 0;
    std::uint8_t nullFlag = 0;
    bool isUserType() const { return typeId == 127 || typeId >= 1024; }
};

bool readHeader(PkDataStream &ds, ValueHeader &h)
{
    std::int32_t typeId = 0;
    std::uint8_t nullFlag = 0;
    ds >> typeId;
    ds >> nullFlag;
    h.typeId = typeId;
    h.nullFlag = nullFlag;
    return ds.status() == PkDataStream::Ok;
}

// 读 UserType 值的 payload（typeName 之后紧跟 XML 字符串）。
bool readUserTypePayload(PkDataStream &ds, PkString &xmlOut)
{
    PkByteArray typeName;
    ds >> typeName; // QByteArray wire：len + bytes；内容不关心
    if (ds.status() != PkDataStream::Ok) {
        return false;
    }
    ds >> xmlOut; // 旧 producer 的 payload 就是 QDataStream<<QString(xml)
    return ds.status() == PkDataStream::Ok;
}

// 旧 AXES 的 XML：<axis tagName= min= max= default= hidden= variable=>
//                 <name lang= value=/>...</axis>。
// 注意：旧 operator>>（KoSvgText.cpp:1001-1090）不读 value 属性，axis.value 保持
// 默认 0 —— 迁移出来的 value 也恒为 0（对齐旧语义）。
bool parseLegacyAxisXml(const PkString &xml, const PkString &hashTag, PkVariantMap &outEntry)
{
    PkXmlDocument doc;
    PkString errMsg;
    int errLine = 0;
    int errColumn = 0;
    if (!doc.setContent(xml, &errMsg, &errLine, &errColumn)) {
        return false;
    }
    const PkXmlElement root = doc.documentElement();
    if (root.tagName() != PkString("axis")) {
        return false;
    }

    PkString tag = root.attribute(PkString("tagName"));
    if (tag.isEmpty()) {
        tag = hashTag; // 旧 producer 的 hash key 就是 tag；XML 里也写 tagName，双保险
    }

    PkVariantMap labels;
    for (PkXmlElement e = root.firstChildElement(); !e.isNull(); e = e.nextSiblingElement()) {
        if (e.tagName() == PkString("name")) {
            const PkString lang = e.attribute(PkString("lang"));
            const PkString value = e.attribute(PkString("value"));
            if (!lang.isEmpty()) {
                labels.emplace(lang, PkVariant(value));
            }
        }
    }

    const double min = root.attribute(PkString("min")).toDouble();
    const double max = root.attribute(PkString("max")).toDouble();
    const double defaultValue = root.attribute(PkString("default")).toDouble();
    const bool hidden = root.attribute(PkString("hidden")) == PkString("true");
    const bool variable = root.attribute(PkString("variable")) == PkString("true");

    outEntry = buildAxisEntry(tag, labels, min, max, 0.0, defaultValue, variable, hidden);
    return true;
}

// 旧 STYLES 的 XML：<style italic= oblique=><coord tag= value=/>...<name lang= value=/>...</style>。
bool parseLegacyStyleXml(const PkString &xml, PkVariantMap &outEntry)
{
    PkXmlDocument doc;
    PkString errMsg;
    int errLine = 0;
    int errColumn = 0;
    if (!doc.setContent(xml, &errMsg, &errLine, &errColumn)) {
        return false;
    }
    const PkXmlElement root = doc.documentElement();
    if (root.tagName() != PkString("style")) {
        return false;
    }

    const bool italic = root.attribute(PkString("italic")) == PkString("true");
    const bool oblique = root.attribute(PkString("oblique")) == PkString("true");

    PkVariantMap coords;
    PkVariantMap labels;
    for (PkXmlElement e = root.firstChildElement(); !e.isNull(); e = e.nextSiblingElement()) {
        const PkString name = e.tagName();
        if (name == PkString("coord")) {
            const PkString tag = e.attribute(PkString("tag"));
            if (!tag.isEmpty()) {
                coords.emplace(tag, PkVariant(e.attribute(PkString("value")).toDouble()));
            }
        } else if (name == PkString("name")) {
            const PkString lang = e.attribute(PkString("lang"));
            const PkString value = e.attribute(PkString("value"));
            if (!lang.isEmpty()) {
                labels.emplace(lang, PkVariant(value));
            }
        }
    }

    outEntry = buildStyleEntry(labels, coords, italic, oblique);
    return true;
}

} // namespace

bool decodeLegacyAxesBlob(const PkByteArray &blob, PkVariantList &outAxes)
{
    PkDataStream ds(blob); // 默认 BigEndian + Qt_5_15，与旧 QDataStream 一致
    ValueHeader top;
    if (!readHeader(ds, top)) {
        return false;
    }
    if (top.nullFlag || top.typeId != static_cast<std::int32_t>(PkVariant::Hash)) {
        return false; // 旧 AXES 是 QVariantHash(28)
    }

    std::uint32_t count = 0;
    ds >> count;
    if (ds.status() != PkDataStream::Ok) {
        return false;
    }

    outAxes.clear();
    for (std::uint32_t i = 0; i < count; ++i) {
        PkString key;
        ds >> key; // QVariantHash 的 key 是 QString
        if (ds.status() != PkDataStream::Ok) {
            return false;
        }
        ValueHeader vh;
        if (!readHeader(ds, vh)) {
            return false;
        }
        if (vh.nullFlag || !vh.isUserType()) {
            return false;
        }
        PkString xml;
        if (!readUserTypePayload(ds, xml)) {
            return false;
        }
        PkVariantMap entry;
        if (!parseLegacyAxisXml(xml, key, entry)) {
            return false;
        }
        outAxes.push_back(PkVariant(entry));
    }
    return ds.status() == PkDataStream::Ok;
}

bool decodeLegacyStylesBlob(const PkByteArray &blob, PkVariantList &outStyles)
{
    PkDataStream ds(blob);
    ValueHeader top;
    if (!readHeader(ds, top)) {
        return false;
    }
    if (top.nullFlag || top.typeId != static_cast<std::int32_t>(PkVariant::List)) {
        return false; // 旧 STYLES 是 QVariantList(9)
    }

    std::uint32_t count = 0;
    ds >> count;
    if (ds.status() != PkDataStream::Ok) {
        return false;
    }

    outStyles.clear();
    for (std::uint32_t i = 0; i < count; ++i) {
        ValueHeader vh;
        if (!readHeader(ds, vh)) {
            return false;
        }
        if (vh.nullFlag || !vh.isUserType()) {
            return false;
        }
        PkString xml;
        if (!readUserTypePayload(ds, xml)) {
            return false;
        }
        PkVariantMap entry;
        if (!parseLegacyStyleXml(xml, entry)) {
            return false;
        }
        outStyles.push_back(PkVariant(entry));
    }
    return ds.status() == PkDataStream::Ok;
}

} // namespace KoFontFamilyMetadata
