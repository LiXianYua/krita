/*
 *  SPDX-FileCopyrightText: 2024 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KoFontFamily.h"
#include "KoFontFamilyMetadata.h"
#include "KoLcLocale.h"
#include <KoMD5Generator.h>
#include <KoSvgTextShape.h>
#include <KoColorBackground.h>
#include <SvgWriter.h>
#include <QPainter>
#include <QBuffer>
#include <QDebug>
#include <KoShapePainter.h>

#include <PkRect.h>
#include <PkDateTime.h>
#include <PkAuxTypes.h>

#include <cstring>
#include <utility>
#include <vector>

struct KoFontFamily::Private {
};

QImage generateImage(const QString &sample, const QString &fontFamily, bool isColor) {
    QSharedPointer<KoSvgTextShape> shape(new KoSvgTextShape);
    shape->setResolution(300, 300);
    shape->setBackground(QSharedPointer<KoColorBackground>(new KoColorBackground(Qt::black)));
    KoSvgTextProperties props = shape->textProperties();
    KoSvgText::CssLengthPercentage fontSize(12.0);
    props.setProperty(KoSvgTextProperties::FontSizeId, QVariant::fromValue(fontSize));
    props.setProperty(KoSvgTextProperties::FontFamiliesId, {fontFamily});
    shape->setPropertiesAtPos(-1, props);
    shape->insertText(0, sample);

    QImage img(256,
               256,
               isColor? QImage::Format_ARGB32: QImage::Format_Grayscale8);
    img.fill(Qt::white);

    KoShapePainter painter;
    painter.setShapes({shape.data()});
    painter.paint(img);

    return img;
}

QString generateSVG(const QString &sample, const QString &fontFamily, QRectF &layoutBox, const QString &lang) {
    QSharedPointer<KoSvgTextShape> shape(new KoSvgTextShape);
    shape->setResolution(300, 300);
    shape->setBackground(QSharedPointer<KoColorBackground>(new KoColorBackground(Qt::black)));
    KoSvgTextProperties props = shape->textProperties();
    props.setProperty(KoSvgTextProperties::FontFamiliesId, {fontFamily});
    KoSvgText::CssLengthPercentage fontSize(12.0);
    props.setProperty(KoSvgTextProperties::FontSizeId, QVariant::fromValue(fontSize));
    if (!lang.isEmpty()) {
        props.setProperty(KoSvgTextProperties::TextLanguage, lang);
    }
    shape->setPropertiesAtPos(-1, props);
    shape->insertText(0, sample);

    layoutBox = shape->selectionBoxes(0, shape->posDown(0)).boundingRect();

    SvgWriter writer({shape->textOutline()});
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    writer.save(buffer, shape->boundingRect().size());
    buffer.close();

    return QString::fromUtf8(buffer.data());
}

namespace {

// R-31 边界层：Qt 类型 → Pk 类型。与 KoCssTextUtils.cpp 的 qStringToPk/pkToQString 一致。
PkString qStringToPk(const QString &s)
{
    const QByteArray u8 = s.toUtf8();
    return PkString::PkFromUtf8(u8.constData(), u8.size());
}

QString pkToQString(const PkString &s)
{
    const std::string u8 = s.PkToUtf8();
    return QString::fromUtf8(u8.data(), int(u8.size()));
}

// PkString → PkByteArray（UTF-8 字节）。旧代码 `QString::toUtf8()` → QByteArray 走
// generateHash(const PkByteArray&) 的「哈希这些字节」语义；PkString 不能隐式转
// PkByteArray，且 generateHash(const PkString&) 是「按文件路径读文件再哈希」——让
// PkString 绑到它上面是静默语义错（会试着打开名为该字符串的文件）。
PkByteArray pkStringToByteArray(const PkString &s)
{
    const std::string u8 = s.PkToUtf8();
    return PkByteArray(u8.data(), int(u8.size()));
}

// 本地化标签 QHash<QLocale,QString> → PkVariantMap<bcp47Name, label>。
// lang 键统一走 R-31 locale 层 bcp47Name()，与旧 producer 的 QLocale::bcp47Name()
// 语义一致（保留冗余子标签的最短形式，见 KoLcLocale.h）。
PkVariantMap localeHashToPkVariantMap(const QHash<QLocale, QString> &names)
{
    PkVariantMap map;
    for (auto it = names.constBegin(); it != names.constEnd(); ++it) {
        map.emplace(KoLc::bcp47Name(qStringToPk(it.key().name())), PkVariant(qStringToPk(it.value())));
    }
    return map;
}

PkVariantMap buildAxisEntryFromQt(const KoSvgText::FontFamilyAxis &axis)
{
    return KoFontFamilyMetadata::buildAxisEntry(
        qStringToPk(axis.tag),
        localeHashToPkVariantMap(axis.localizedLabels),
        axis.min, axis.max, axis.value, axis.defaultValue,
        axis.variableAxis, axis.axisHidden);
}

PkVariantMap buildStyleEntryFromQt(const KoSvgText::FontFamilyStyleInfo &style)
{
    PkVariantMap coords;
    for (auto it = style.instanceCoords.constBegin(); it != style.instanceCoords.constEnd(); ++it) {
        coords.emplace(qStringToPk(it.key()), PkVariant(double(it.value())));
    }
    return KoFontFamilyMetadata::buildStyleEntry(
        localeHashToPkVariantMap(style.localizedLabels),
        coords, style.isItalic, style.isOblique);
}

// QImage → PkImage。PkImage::Format 数值与 QImage::Format 顺序一致（见
// pk/image/PkImage.h，自 Format_Invalid 起逐项对应），用 static_cast 直转；
// 像素逐 scanLine 拷贝；索引色表也拷贝。
PkImage qimageToPkImage(const QImage &img)
{
    PkImage out(static_cast<PkImage::Format>(img.format()), img.width(), img.height());
    for (int y = 0; y < img.height(); ++y) {
        const uchar *src = img.constScanLine(y);
        uint8_t *dst = out.scanLine(y);
        std::memcpy(dst, src, size_t(img.bytesPerLine()));
    }
    if (img.colorCount() > 0) {
        std::vector<uint32_t> table;
        table.reserve(size_t(img.colorCount()));
        for (int i = 0; i < img.colorCount(); ++i) {
            table.push_back(img.color(i));
        }
        out.setColorTable(table);
    }
    return out;
}

} // namespace

KoFontFamily::KoFontFamily(KoFontFamilyWWSRepresentation representation)
    : KoResource(qStringToPk(representation.fontFamilyName))
    , d(new Private())

{
    setName(qStringToPk(representation.fontFamilyName));
    addMetaData(KoFontFamilyMetadata::KEY_TYPOGRAPHIC_NAME, PkVariant(qStringToPk(representation.typographicFamilyName)));
    addMetaData(KoFontFamilyMetadata::KEY_LOCALIZED_FONT_FAMILY, localeHashToPkVariantMap(representation.localizedFontFamilyNames));
    addMetaData(KoFontFamilyMetadata::KEY_LOCALIZED_TYPOGRAPHIC_NAME, localeHashToPkVariantMap(representation.localizedTypographicFamily));
    addMetaData(KoFontFamilyMetadata::KEY_LOCALIZED_TYPOGRAPHIC_STYLE, localeHashToPkVariantMap(representation.localizedTypographicStyles));

    addMetaData(KoFontFamilyMetadata::KEY_LAST_MODIFIED,
                PkVariant::PkFromDateTime(
                    PkDateTime::fromSecsSinceEpoch(representation.lastModified.toSecsSinceEpoch()),
                    PkVariant::DateTimeSpec::LocalTime));

    PkVariantMap samples;
    for (auto it = representation.sampleStrings.constBegin(); it != representation.sampleStrings.constEnd(); ++it) {
        samples.emplace(qStringToPk(it.key()), PkVariant(qStringToPk(it.value())));
    }
    addMetaData(KoFontFamilyMetadata::KEY_SAMPLE_STRING, samples);
    PkVariantList supportedLanguages;
    for (const QLocale &l : representation.supportedLanguages) {
        supportedLanguages.push_back(PkVariant(KoLc::bcp47Name(qStringToPk(l.name()))));
    }
    addMetaData(KoFontFamilyMetadata::KEY_SUPPORTED_LANGUAGES, supportedLanguages);

    addMetaData(KoFontFamilyMetadata::KEY_FONT_TYPE, PkVariant(int(representation.type)));
    addMetaData(KoFontFamilyMetadata::KEY_IS_VARIABLE, PkVariant(representation.isVariable));
    addMetaData(KoFontFamilyMetadata::KEY_COLOR_BITMAP, PkVariant(representation.colorBitMap));
    addMetaData(KoFontFamilyMetadata::KEY_COLOR_CLRV0, PkVariant(representation.colorClrV0));
    addMetaData(KoFontFamilyMetadata::KEY_COLOR_CLRV1, PkVariant(representation.colorClrV1));
    addMetaData(KoFontFamilyMetadata::KEY_COLOR_SVG, PkVariant(representation.colorSVG));
    PkVariantList axes;
    for (auto it = representation.axes.constBegin(); it != representation.axes.constEnd(); ++it) {
        axes.push_back(PkVariant(buildAxisEntryFromQt(it.value())));
    }
    addMetaData(KoFontFamilyMetadata::KEY_AXES, axes);
    PkVariantList styles;
    for (const KoSvgText::FontFamilyStyleInfo &style : representation.styles) {
        styles.push_back(PkVariant(buildStyleEntryFromQt(style)));
    }
    addMetaData(KoFontFamilyMetadata::KEY_STYLES, styles);
    setMD5Sum(KoMD5Generator::generateHash(pkStringToByteArray(qStringToPk(representation.fontFamilyName))));
    setValid(true);
}

KoFontFamily::KoFontFamily(const PkString &filename)
    :KoResource(filename)
{
    setMD5Sum(KoMD5Generator::generateHash(pkStringToByteArray(ResourceType::FontFamilies)));
    setValid(false);
}

KoFontFamily::~KoFontFamily()
{
}

KoFontFamily::KoFontFamily(const KoFontFamily &rhs)
    : KoResource(PkString())
    , d(new Private(*rhs.d))
{
    setFilename(rhs.filename());
    // ⚠ 保持原行为：原实现这里取的也是「新对象」的 metadata()（空），复制循环
    // 实际不拷贝任何元数据——见报告「隐患」节（clone 路径丢 SAMPLE 等元数据）。
    PkMap<PkString, PkVariant> meta = metadata();
    for (auto it = meta.constBegin(); it != meta.constEnd(); ++it) {
        addMetaData(it.key(), it.value());
    }
    setValid(true);
}

KoResourceSP KoFontFamily::clone() const
{
    return KoResourceSP(new KoFontFamily(*this));
}

bool KoFontFamily::loadFromDevice(PkStream *dev, KisResourcesInterfaceSP resourcesInterface)
{
    Q_UNUSED(dev)
    Q_UNUSED(resourcesInterface);
    return false;
}

bool KoFontFamily::isSerializable() const
{
    return false;
}

std::pair<PkString, PkString> KoFontFamily::resourceType() const
{
    return std::pair<PkString, PkString>(ResourceType::FontFamilies, PkString());
}

void KoFontFamily::updateThumbnail()
{
    const PkVariantMap samples = metadata().value(KoFontFamilyMetadata::KEY_SAMPLE_STRING).toMap();
    PkVariantMap sampleSVG;
    PkVariantMap sampleSVGBbox;

    for (auto it = samples.begin(); it != samples.end(); ++it) {
        const QString sample = pkToQString(it->second.toString());
        QRectF sampleBBox;
        const QString key = pkToQString(it->first);
        const QString lang = key.startsWith("l_")? key.mid(2): QString();
        sampleSVG.emplace(it->first, PkVariant(qStringToPk(generateSVG(sample, pkToQString(filename()), sampleBBox, lang))));
        sampleSVGBbox.emplace(it->first, PkVariant(PkRectF(sampleBBox.x(), sampleBBox.y(), sampleBBox.width(), sampleBBox.height())));
    }

    addMetaData(KoFontFamilyMetadata::KEY_SAMPLE_SVG, sampleSVG);
    addMetaData(KoFontFamilyMetadata::KEY_SAMPLE_BBOX, sampleSVGBbox);
    QString sample;
    if (samples.isEmpty()) {
        sample = QStringLiteral("AaBbGg");
    } else {
        // ⚠ 行为归一化：旧 getter 用 .toHash()，对 Map(8) 类型的 SAMPLE_STRING 恒返回
        // 空，缩略图实际总用默认 "AaBbGg"；这里改 .toMap()，真正用上存的样本——
        // 见报告「隐患」节。
        const auto itLatn = samples.find(PkString("s_Latn"));
        sample = (itLatn != samples.end())
                ? pkToQString(itLatn->second.toString())
                : pkToQString(samples.begin()->second.toString());
    }
    bool isColor = (metadata().value(KoFontFamilyMetadata::KEY_COLOR_BITMAP).toBool() || metadata().value(KoFontFamilyMetadata::KEY_COLOR_CLRV0).toBool());
    setImage(qimageToPkImage(generateImage(sample, pkToQString(filename()), isColor)));
}

QString KoFontFamily::typographicFamily() const
{
    return pkToQString(metadata().value(KoFontFamilyMetadata::KEY_TYPOGRAPHIC_NAME).toString());
}

QString KoFontFamily::translatedFontName(QStringList locales) const
{
    const PkVariantMap names = metadata().value(KoFontFamilyMetadata::KEY_LOCALIZED_FONT_FAMILY).toMap();
    QString name = pkToQString(filename());
    for (const QString &locale : locales) {
        const auto it = names.find(qStringToPk(locale));
        if (it != names.end()) {
            name = pkToQString(it->second.toString());
            break;
        }
    }
    return name;
}

bool KoFontFamily::isVariable() const
{
    return metadata().value(KoFontFamilyMetadata::KEY_IS_VARIABLE).toBool();
}

bool KoFontFamily::colorBitmap() const
{
    return metadata().value(KoFontFamilyMetadata::KEY_COLOR_BITMAP).toBool();
}

bool KoFontFamily::colorClrV0() const
{
    return metadata().value(KoFontFamilyMetadata::KEY_COLOR_CLRV0).toBool();
}

bool KoFontFamily::colorClrV1() const
{
    return metadata().value(KoFontFamilyMetadata::KEY_COLOR_CLRV1).toBool();
}

bool KoFontFamily::colorSVG() const
{
    return metadata().value(KoFontFamilyMetadata::KEY_COLOR_SVG).toBool();
}

QList<KoSvgText::FontFamilyAxis> KoFontFamily::axes() const
{
    QList<KoSvgText::FontFamilyAxis> converted;
    const PkVariantList axes = metadata().value(KoFontFamilyMetadata::KEY_AXES).toList();
    for (const PkVariant &val : axes) {
        const auto entry = KoFontFamilyMetadata::parseAxisEntry(val.toMap());
        if (!entry) {
            // R-31：显式暴露不可解码条目（版本不符/类型错），不静默丢弃；原始
            // payload 由 DB 层逐字节保留（本 getter 只读不解）。
            qWarning() << "KoFontFamily::axes(): 存在不可解码的 AXES 条目，已跳过";
            continue;
        }
        KoSvgText::FontFamilyAxis axis;
        axis.tag = pkToQString(entry->tag);
        for (auto it = entry->localizedLabels.begin(); it != entry->localizedLabels.end(); ++it) {
            axis.localizedLabels.insert(QLocale(pkToQString(it->first)), pkToQString(it->second.toString()));
        }
        axis.min = entry->min;
        axis.max = entry->max;
        axis.value = entry->value;
        axis.defaultValue = entry->defaultValue;
        axis.variableAxis = entry->variableAxis;
        axis.axisHidden = entry->axisHidden;
        converted.append(axis);
    }
    return converted;
}

QList<KoSvgText::FontFamilyStyleInfo> KoFontFamily::styles() const
{
    QList<KoSvgText::FontFamilyStyleInfo> converted;
    const PkVariantList styles = metadata().value(KoFontFamilyMetadata::KEY_STYLES).toList();
    for (const PkVariant &val : styles) {
        const auto entry = KoFontFamilyMetadata::parseStyleEntry(val.toMap());
        if (!entry) {
            qWarning() << "KoFontFamily::styles(): 存在不可解码的 STYLES 条目，已跳过";
            continue;
        }
        KoSvgText::FontFamilyStyleInfo style;
        for (auto it = entry->localizedLabels.begin(); it != entry->localizedLabels.end(); ++it) {
            style.localizedLabels.insert(QLocale(pkToQString(it->first)), pkToQString(it->second.toString()));
        }
        for (auto it = entry->instanceCoords.begin(); it != entry->instanceCoords.end(); ++it) {
            style.instanceCoords.insert(pkToQString(it->first), float(it->second.toDouble()));
        }
        style.isItalic = entry->isItalic;
        style.isOblique = entry->isOblique;
        converted.append(style);
    }
    return converted;
}

QDateTime KoFontFamily::lastModified() const
{
    const PkDateTime dt = metadata().value(KoFontFamilyMetadata::KEY_LAST_MODIFIED).toDateTime();
    if (!dt.isValid()) {
        return QDateTime();
    }
    return QDateTime::fromSecsSinceEpoch(dt.toSecsSinceEpoch(), Qt::LocalTime);
}
