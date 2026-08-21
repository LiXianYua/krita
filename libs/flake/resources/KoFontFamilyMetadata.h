/*
 *  SPDX-FileCopyrightText: 2026 S-08 (R-31)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KOFONTFAMILYMETADATA_H
#define KOFONTFAMILYMETADATA_H

#include <PkString.h>
#include <PkVariant.h>
#include <PkAuxTypes.h>

#include <optional>

// S-08 R-31：字体 metadata 的稳定 built-in 表示（PkVariant 内置类型组合），取代
// 旧 KoSvgText::FontFamilyAxis/FontFamilyStyleInfo UserType wire。
//
// 纯 Pk、零 Qt：本文件可进薄壳编译（判据③④）。Qt 边界层（KoFontFamily.cpp）负责
// QLocale/QString → Pk 的转换，本层只关心「条目长什么样、怎么编解码、怎么从旧
// UserType blob 迁移」。
//
// 表示形状（版本化，见 FORMAT_VERSION；全部 PkVariant 内置类型，无 UserType，
// PkDataStream 可 round-trip）：
//   AXES   → PkVariantList of PkVariantMap，key：version(int)、tag(PkString)、
//            localizedLabels(PkVariantMap<lang,label>)、min/max/value/
//            defaultValue(double)、variableAxis/axisHidden(bool)
//   STYLES → PkVariantList of PkVariantMap，key：version(int)、localizedLabels(
//            PkVariantMap<lang,label>)、instanceCoords(PkVariantMap<tag,double>)、
//            isItalic/isOblique(bool)
//   LOCALIZED_FONT_FAMILY / LOCALIZED_TYPOGRAPHIC_NAME /
//   LOCALIZED_TYPOGRAPHIC_STYLE → PkVariantMap<lang, PkString>（bcp47Name 经
//   KoLcLocale，Qt 边界层转换）
//
// 迁移/回滚/版本策略见 .superpowers/sdd/S-08/task-4-report.md §built-in。
namespace KoFontFamilyMetadata {

// 元数据 key 名（DB 里存 Base64(PkDataStream blob) 的键；key 名是跨版本契约，
// 只在 major 版本升级时允许变，且需迁移）。用 constexpr const char* 避免任何
// 动态初始化顺序问题；所有消费点经 PkString 隐式构造。
inline constexpr const char *KEY_AXES = "axes";
inline constexpr const char *KEY_STYLES = "styles";
inline constexpr const char *KEY_LOCALIZED_FONT_FAMILY = "localized_font_family";
inline constexpr const char *KEY_LOCALIZED_TYPOGRAPHIC_NAME = "localized_typographic_name";
inline constexpr const char *KEY_LOCALIZED_TYPOGRAPHIC_STYLE = "localized_typographic_style";
inline constexpr const char *KEY_TYPOGRAPHIC_NAME = "typographic_name";
inline constexpr const char *KEY_FONT_TYPE = "font_type";
inline constexpr const char *KEY_IS_VARIABLE = "is_variable";
inline constexpr const char *KEY_COLOR_BITMAP = "color_bitmap";
inline constexpr const char *KEY_COLOR_CLRV0 = "color_clrv0";
inline constexpr const char *KEY_COLOR_CLRV1 = "color_clrv1";
inline constexpr const char *KEY_COLOR_SVG = "color_svg";
inline constexpr const char *KEY_SAMPLE_STRING = "sample_string";
inline constexpr const char *KEY_SAMPLE_SVG = "sample_svg";
inline constexpr const char *KEY_SAMPLE_BBOX = "sample_bbox";
inline constexpr const char *KEY_LAST_MODIFIED = "last_modified";
inline constexpr const char *KEY_SUPPORTED_LANGUAGES = "supported_languages";

// built-in 格式版本。AXES/STYLES 每个条目都带 version int；解码时版本不符按
// 不可解码处理（R-31：显式暴露，不静默降级）。
inline constexpr int FORMAT_VERSION = 1;

// AXES/STYLES 条目内 key。
inline constexpr const char *ENTRY_VERSION = "version";
inline constexpr const char *ENTRY_TAG = "tag";
inline constexpr const char *ENTRY_LOCALIZED_LABELS = "localizedLabels";
inline constexpr const char *ENTRY_INSTANCE_COORDS = "instanceCoords";
inline constexpr const char *ENTRY_IS_ITALIC = "isItalic";
inline constexpr const char *ENTRY_IS_OBLIQUE = "isOblique";
inline constexpr const char *ENTRY_MIN = "min";
inline constexpr const char *ENTRY_MAX = "max";
inline constexpr const char *ENTRY_VALUE = "value";
inline constexpr const char *ENTRY_DEFAULT_VALUE = "defaultValue";
inline constexpr const char *ENTRY_VARIABLE_AXIS = "variableAxis";
inline constexpr const char *ENTRY_AXIS_HIDDEN = "axisHidden";

// 解码出的纯 Pk 轴条目（Qt 边界层再转 KoSvgText::FontFamilyAxis）。
struct PkAxisEntry {
    PkString tag;
    PkVariantMap localizedLabels; // lang → label（PkString）
    double min = -1;
    double max = -1;
    double value = 0;
    double defaultValue = 0;
    bool variableAxis = false;
    bool axisHidden = false;
};

struct PkStyleEntry {
    PkVariantMap localizedLabels; // lang → label（PkString）
    PkVariantMap instanceCoords;  // tag → double
    bool isItalic = false;
    bool isOblique = false;
};

// ── built-in 编码（生产者用；Qt 边界层已把 QLocale/QString 换成 Pk）──
PkVariantMap buildAxisEntry(const PkString &tag,
                            const PkVariantMap &localizedLabels,
                            double min, double max, double value, double defaultValue,
                            bool variableAxis, bool axisHidden);
PkVariantMap buildStyleEntry(const PkVariantMap &localizedLabels,
                             const PkVariantMap &instanceCoords,
                             bool isItalic, bool isOblique);

// ── built-in 解码（消费者用）──
// std::nullopt = 不可解码（version 不符 / 缺关键 key / 类型错）。调用方不得静默
// 丢弃：R-31 要求显式暴露不可解码状态，原始 payload 由 DB 层逐字节保留。
std::optional<PkAxisEntry> parseAxisEntry(const PkVariantMap &entry);
std::optional<PkStyleEntry> parseStyleEntry(const PkVariantMap &entry);

// ── 旧 UserType 迁移 ──
// 旧 wire：AXES = QVariantHash(28)、STYLES = QVariantList(9)，条目值都是
// qRegisterMetaTypeStreamOperators 注册的 FontFamilyAxis/FontFamilyStyleInfo
// UserType（Qt5: typeId>=1024；Qt4: typeId==127），payload = QDataStream 序列化
// 的 QDomDocument.toString(0) XML 字符串（KoSvgText.cpp:1001-1090 的 operator<<）。
// 返回 false = blob 不可解码（容器类型不符 / 字段读越界 / XML 解析失败）——
// 调用方（DB 层迁移器）保留原始 payload 字节，不写回。
bool decodeLegacyAxesBlob(const PkByteArray &blob, PkVariantList &outAxes);
bool decodeLegacyStylesBlob(const PkByteArray &blob, PkVariantList &outStyles);

} // namespace KoFontFamilyMetadata

#endif // KOFONTFAMILYMETADATA_H
