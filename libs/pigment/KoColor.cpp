/*
 *  SPDX-FileCopyrightText: 2005 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2007 Thomas Zander <zander@kde.org>
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/

// PkXmlCompat.h 必须先于 KoColor.h：它把 KoColorSpace.h 不自激活的 compat 宏
// （那批 KoColorSpace.h 用到却不自 include 的 compat 宏，清单见 PkXmlCompat.h）先激活，KoColor.h 才拉得到
// KoColorSpaceRegistry.h → KoColorSpace.h，那些纯虚签名才能在编译期改写成
// Pk 类型（fromQColor/toQColor/colorToXML/colorFromXML 等）。
#include <PkXmlCompat.h>

#include "KoColor.h"

#include <charconv>
#include <cstring>
#include <string>
#include <system_error>

#include <PkStringList.h>
#include <PkXmlDocument.h>

#include "DebugPigment.h"

#include "KoColorModelStandardIds.h"
#include "KoColorProfile.h"
#include "KoColorSpace.h"
#include "KoColorSpaceRegistry.h"
#include "KoChannelInfo.h"
#include "kis_assert.h"
#include "kis_dom_utils.h"

#include <KoConfig.h>
#ifdef HAVE_OPENEXR
#include <half.h>
#endif

namespace {

// 对齐 Qt 的 double→string 默认 'g' 6 位格式：std::to_chars general 格式，
// locale 无关，与 C locale 的 snprintf('g') 对齐。±0.0 → "0"。
PkString pkNumberToString(double value, int precision)
{
    if (value == 0.0) {
        return PkString("0");
    }
    char tmp[64];
    const std::to_chars_result r = std::to_chars(tmp, tmp + sizeof(tmp), value,
                                                  std::chars_format::general, precision);
    if (r.ec != std::errc()) {
        return PkString();
    }
    return PkString::PkFromUtf8(tmp, static_cast<int>(r.ptr - tmp));
}

struct DefaultKoColorInitializer
{
    DefaultKoColorInitializer() {
        const KoColorSpace *defaultColorSpace = KoColorSpaceRegistry::instance()->rgb16(0);
        KIS_ASSERT(defaultColorSpace);

        value = new KoColor(Qt::black, defaultColorSpace);
#ifndef NODEBUG
#ifndef QT_NO_DEBUG
        // warn about rather expensive checks in assertPermanentColorspace().
        qWarning() << "KoColor debug runtime checks are active.";
#endif
#endif
    }

    ~DefaultKoColorInitializer() {
        delete value;
    }

    KoColor *value = 0;
};

DefaultKoColorInitializer &defaultKoColorInitializer()
{
    static DefaultKoColorInitializer s;
    return s;
}

}

KoColor::KoColor()
    : m_colorSpace(defaultKoColorInitializer().value->m_colorSpace)
    , m_size(defaultKoColorInitializer().value->m_size)
{
    memcpy(m_data, defaultKoColorInitializer().value->m_data, m_size);
}

KoColor::KoColor(const KoColorSpace * colorSpace)
{
    Q_ASSERT(colorSpace);
    m_colorSpace = KoColorSpaceRegistry::instance()->permanentColorspace(colorSpace);
    m_size = m_colorSpace->pixelSize();
    Q_ASSERT(m_size <= MAX_PIXEL_SIZE);
    memset(m_data, 0, m_size);
}

KoColor::KoColor(const PkColor & color, const KoColorSpace * colorSpace)
{
    Q_ASSERT(color.isValid());
    Q_ASSERT(colorSpace);
    m_colorSpace = KoColorSpaceRegistry::instance()->permanentColorspace(colorSpace);

    m_size = m_colorSpace->pixelSize();
    Q_ASSERT(m_size <= MAX_PIXEL_SIZE);
    memset(m_data, 0, m_size);

    m_colorSpace->fromQColor(color, m_data);
}

KoColor::KoColor(const quint8 * data, const KoColorSpace * colorSpace)
{
    Q_ASSERT(colorSpace);
    Q_ASSERT(data);
    m_colorSpace = KoColorSpaceRegistry::instance()->permanentColorspace(colorSpace);
    m_size = m_colorSpace->pixelSize();
    Q_ASSERT(m_size <= MAX_PIXEL_SIZE);
    memmove(m_data, data, m_size);
}


KoColor::KoColor(const KoColor &src, const KoColorSpace * colorSpace)
{
    Q_ASSERT(colorSpace);
    m_colorSpace = KoColorSpaceRegistry::instance()->permanentColorspace(colorSpace);
    m_size = m_colorSpace->pixelSize();
    Q_ASSERT(m_size <= MAX_PIXEL_SIZE);
    memset(m_data, 0, m_size);

    src.colorSpace()->convertPixelsTo(src.m_data, m_data, colorSpace, 1, KoColorConversionTransformation::internalRenderingIntent(), KoColorConversionTransformation::internalConversionFlags());
}

bool KoColor::operator==(const KoColor &other) const {
    if (*colorSpace() != *other.colorSpace()) {
        return false;
    }
    if (m_size != other.m_size) {
        return false;
    }
    return memcmp(m_data, other.m_data, m_size) == 0;
}

void KoColor::convertTo(const KoColorSpace * cs, KoColorConversionTransformation::Intent renderingIntent, KoColorConversionTransformation::ConversionFlags conversionFlags)
{
    //dbgPigment <<"Our colormodel:" << d->colorSpace->id().name()
    //      << ", new colormodel: " << cs->id().name() << "\n";

    if (*m_colorSpace == *cs)
        return;

    quint8 data[MAX_PIXEL_SIZE];
    const size_t size = cs->pixelSize();
    Q_ASSERT(size <= MAX_PIXEL_SIZE);
    memset(data, 0, size);

    m_colorSpace->convertPixelsTo(m_data, data, cs, 1, renderingIntent, conversionFlags);

    memcpy(m_data, data, size);
    m_size = size;
    m_colorSpace = KoColorSpaceRegistry::instance()->permanentColorspace(cs);
}

void KoColor::convertTo(const KoColorSpace * cs)
{
    convertTo(cs,
              KoColorConversionTransformation::internalRenderingIntent(),
              KoColorConversionTransformation::internalConversionFlags());
}

KoColor KoColor::convertedTo(const KoColorSpace *cs, KoColorConversionTransformation::Intent renderingIntent, KoColorConversionTransformation::ConversionFlags conversionFlags) const
{
    KoColor result(*this);
    result.convertTo(cs, renderingIntent, conversionFlags);
    return result;
}

KoColor KoColor::convertedTo(const KoColorSpace *cs) const
{
    return convertedTo(cs,
                       KoColorConversionTransformation::internalRenderingIntent(),
                       KoColorConversionTransformation::internalConversionFlags());
}

void KoColor::setProfile(const KoColorProfile *profile)
{
    const KoColorSpace *dstColorSpace =
            KoColorSpaceRegistry::instance()->colorSpace(colorSpace()->colorModelId().id(), colorSpace()->colorDepthId().id(), profile);
    if (!dstColorSpace) return;

    m_colorSpace = KoColorSpaceRegistry::instance()->permanentColorspace(dstColorSpace);
}

void KoColor::setColor(const quint8 * data, const KoColorSpace * colorSpace)
{
    Q_ASSERT(colorSpace);

    m_size = colorSpace->pixelSize();
    Q_ASSERT(m_size <= MAX_PIXEL_SIZE);

    memcpy(m_data, data, m_size);
    m_colorSpace = KoColorSpaceRegistry::instance()->permanentColorspace(colorSpace);
}

// To save the user the trouble of doing color->colorSpace()->toQColor(color->data(), &c, &a, profile
void KoColor::toQColor(PkColor *c) const
{
    Q_ASSERT(c);
    if (m_colorSpace) {
        m_colorSpace->toQColor(m_data, c);
    }
}

PkColor KoColor::toQColor() const
{
    PkColor c;
    toQColor(&c);
    return c;
}

void KoColor::fromQColor(const PkColor& c)
{
    if (m_colorSpace) {
        m_colorSpace->fromQColor(c, m_data);
    }
}

void KoColor::subtract(const KoColor &value)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(*m_colorSpace == *value.colorSpace());

    PkVector<float> channels1(m_colorSpace->channelCount());
    PkVector<float> channels2(m_colorSpace->channelCount());

    m_colorSpace->normalisedChannelsValue(m_data, channels1);
    m_colorSpace->normalisedChannelsValue(value.data(), channels2);

    for (int i = 0; i < channels1.size(); i++) {
        channels1[i] -= channels2[i];
    }

    m_colorSpace->fromNormalisedChannelsValue(m_data, channels1);
}

KoColor KoColor::subtracted(const KoColor &value) const
{
    KoColor result(*this);
    result.subtract(value);
    return result;
}

void KoColor::add(const KoColor &value)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(*m_colorSpace == *value.colorSpace());

    PkVector<float> channels1(m_colorSpace->channelCount());
    PkVector<float> channels2(m_colorSpace->channelCount());

    m_colorSpace->normalisedChannelsValue(m_data, channels1);
    m_colorSpace->normalisedChannelsValue(value.data(), channels2);

    for (int i = 0; i < channels1.size(); i++) {
        channels1[i] += channels2[i];
    }

    m_colorSpace->fromNormalisedChannelsValue(m_data, channels1);
}

KoColor KoColor::added(const KoColor &value) const
{
    KoColor result(*this);
    result.add(value);
    return result;
}

#ifndef NDEBUG
void KoColor::dump() const
{
    dbgPigment << "KoColor (" << this << ")," << m_colorSpace->id();
    PkList<KoChannelInfo *> channels = m_colorSpace->channels();

    for (auto it = channels.constBegin(); it != channels.constEnd(); ++it) {
        KoChannelInfo * ch = (*it);
        // XXX: setNum always takes a byte.
        if (ch->size() == sizeof(quint8)) {
            // Byte
            dbgPigment << "Channel (byte):" << ch->name() << ":" << std::to_string(int(m_data[ch->pos()]));
        } else if (ch->size() == sizeof(quint16)) {
            // Short (may also by an nvidia half)
            dbgPigment << "Channel (short):" << ch->name() << ":" << std::to_string(*((const quint16 *)(m_data+ch->pos())));
        } else if (ch->size() == sizeof(quint32)) {
            // Integer (may also be float... Find out how to distinguish these!)
            dbgPigment << "Channel (int):" << ch->name() << ":" << std::to_string(*((const quint32 *)(m_data+ch->pos())));
        }
    }
}
#endif

void KoColor::fromKoColor(const KoColor& src)
{
    src.colorSpace()->convertPixelsTo(src.m_data, m_data, colorSpace(), 1, KoColorConversionTransformation::internalRenderingIntent(), KoColorConversionTransformation::internalConversionFlags());
}

const KoColorProfile *KoColor::profile() const
{
    return m_colorSpace->profile();
}

void KoColor::toXML(PkXmlDocument& doc, PkXmlElement& colorElt) const
{
    m_colorSpace->colorToXML(m_data, doc, colorElt);

    for (PkString key : m_metadata.keys()) {

        PkXmlElement e = doc.createElement("metadata");
        e.setAttribute("name", key);
        PkVariant v = m_metadata.value(key);
        e.setAttribute("type", v.typeName());

        PkString attrName = "value";
        if (v.type() == PkVariant::String) {
            e.setAttribute(attrName, v.toString());
            e.setAttribute("type", "string");
        } else if (v.type() == PkVariant::Int) {
            e.setAttribute(attrName, KisDomUtils::numberToString(v.toInt()));
        } else if (v.type() == PkVariant::Double) {
            e.setAttribute(attrName, KisDomUtils::numberToString(v.toDouble(), 6));
        } else if (v.type() == PkVariant::Bool) {
            e.setAttribute(attrName, KisDomUtils::numberToString(v.toBool()));
        } else {
            qWarning() << "no KoColor serialization for variant type:" << static_cast<int>(v.type());
        }
        colorElt.appendChild(e);
    }

}

void KoColor::setOpacity(quint8 alpha)
{
    m_colorSpace->setOpacity(m_data, alpha, 1);
}
void KoColor::setOpacity(qreal alpha)
{
    m_colorSpace->setOpacity(m_data, alpha, 1);
}
quint8 KoColor::opacityU8() const
{
    return m_colorSpace->opacityU8(m_data);
}
qreal KoColor::opacityF() const
{
    return m_colorSpace->opacityF(m_data);
}

KoColor KoColor::fromXML(const PkXmlElement& elt, const PkString& channelDepthId)
{
    bool ok;
    return fromXML(elt, channelDepthId, &ok);
}

KoColor KoColor::fromXML(const PkXmlElement &elt, const PkString &channelDepthId, bool *ok)
{
    *ok = true;

    PkString modelId;
    PkString modelName = elt.tagName();
    if (modelName == "CMYK") {
        modelId = CMYKAColorModelID.id();
    } else if (modelName == "RGB") {
        modelId = RGBAColorModelID.id();
    } else if (modelName == "sRGB") {
        modelId = RGBAColorModelID.id();
    } else if (modelName == "Lab") {
        modelId = LABAColorModelID.id();
    } else if (modelName == "XYZ") {
        modelId = XYZAColorModelID.id();
    } else if (modelName == "Gray") {
        modelId = GrayAColorModelID.id();
    } else if (modelName == "YCbCr") {
        modelId = YCbCrAColorModelID.id();
    }

    KoColorSpaceRegistry *colorSpaceRegistry = KoColorSpaceRegistry::instance();

    PkString profileName;
    if (modelName == "sRGB") {
        const KoColorProfile *profile = colorSpaceRegistry->p709SRGBProfile();
        if (profile) {
            profileName = profile->name();
        }
    } else {
        profileName = elt.attribute("space", "");
        if (!colorSpaceRegistry->profileByName(profileName)) {
            profileName = PkString();
        }
    }

    const KoColorSpace* cs = colorSpaceRegistry->colorSpace(modelId, channelDepthId, profileName);
    if (!cs) {
        PkList<KoID> list = colorSpaceRegistry->colorDepthList(modelId, KoColorSpaceRegistry::AllColorSpaces);
        if (!list.empty()) {
            cs = colorSpaceRegistry->colorSpace(modelId, list[0].id(), profileName);
        }
    }

    if (!cs) {
        *ok = false;
        return KoColor();
    }

    KoColor c(cs);
    // TODO: Provide a way for colorFromXML() to notify the caller if parsing failed.
    //       Currently it returns default values on failure.
    cs->colorFromXML(c.data(), elt);

    PkXmlElement e = elt.nextSiblingElement("metadata");
    for (; !e.isNull(); e = e.nextSiblingElement("metadata")) {
        const PkString name = e.attribute("name");
        const PkString type = e.attribute("type");
        const PkString value = e.attribute("value");

        PkVariant v;
        if (type == "string") {
            v = KisDomUtils::toString(value);
        } else if (type == "int") {
            v = KisDomUtils::toInt(value);
        } else if (type == "double") {
            v = KisDomUtils::toDouble(value);
        }  else if (type == "bool") {
            v = KisDomUtils::toInt(value);
        } else {
            continue;
        }

        c.addMetadata(name , v);
    }

    return c;
}

PkString KoColor::toXML() const
{
    PkXmlDocument cdataDoc("color");
    PkXmlElement cdataRoot = cdataDoc.createElement("color");
    cdataDoc.appendChild(cdataRoot);
    cdataRoot.setAttribute("channeldepth", colorSpace()->colorDepthId().id());
    toXML(cdataDoc, cdataRoot);
    return cdataDoc.toString();
}

KoColor KoColor::fromXML(const PkString &xml)
{
    KoColor c;
    PkXmlDocument doc;
    if (!doc.setContent(xml)) {
        return c;
    }

    PkXmlElement root = doc.documentElement();
    PkXmlElement child = root.firstChildElement();
    PkString channelDepthID = root.attribute("channeldepth", Integer16BitsColorDepthID.id());

    bool ok;
    if (child.hasAttribute("space") || child.tagName().toLower() == "srgb") {
        c = KoColor::fromXML(child, channelDepthID, &ok);
    } else if (root.hasAttribute("space") || root.tagName().toLower() == "srgb"){
        c = KoColor::fromXML(root, channelDepthID, &ok);
    } else {
        qWarning() << "Cannot parse color from xml" << xml;
    }

    return c;
}

PkString KoColor::toSVG11(PkHash<PkString, const KoColorProfile *> *profileList) const
{
    PkStringList colorDefinitions;
    colorDefinitions.append(toQColor().name());

    PkVector<float> channelValues(colorSpace()->channelCount());
    channelValues.fill(0.0);
    colorSpace()->normalisedChannelsValue(data(), channelValues);

    bool sRGB = false;
    if (colorSpace() && colorSpace()->profile()
            && colorSpace()->profile()->getColorPrimaries() == ColorPrimaries::PRIMARIES_ITU_R_BT_709_5
            && colorSpace()->profile()->getTransferCharacteristics() != TransferCharacteristics::TRC_LINEAR) {
        sRGB = true;
    }

    // We don't write a icc-color definition for XYZ and 8bit sRGB.
    if (!(sRGB && colorSpace()->colorDepthId() == Integer8BitsColorDepthID) &&
            colorSpace()->colorModelId() != XYZAColorModelID) {
        PkStringList iccColor;
        PkString csName = colorSpace()->profile()->name();
        // remove forbidden characters
        // https://www.w3.org/TR/SVG11/types.html#DataTypeName
        PkString cleanName;
        for (int i = 0; i < csName.size(); ++i) {
            const char16_t ch = csName.at(i);
            if (ch == u'(' || ch == u')' || ch == u',' || ch == u' ' || ch == u'\t'
                    || ch == u'\n' || ch == u'\r' || ch == u'\f' || ch == u'\v') {
                continue;
            }
            cleanName += pkCharToString(ch);
        }
        csName = cleanName;

        //reuse existing name if possible. We're looking for the color profile, because svg doesn't care about depth.
        csName = profileList->key(colorSpace()->profile(), csName);

        if (sRGB) {
            csName = "sRGB";
        }

        iccColor.append(csName);

        if (colorSpace()->colorModelId() == LABAColorModelID) {
            PkXmlDocument doc;
            PkXmlElement el = doc.createElement("color");
            toXML(doc, el);
            PkXmlElement lab = el.firstChildElement();
            iccColor.append(lab.attribute("L", "0.0"));
            iccColor.append(lab.attribute("a", "0.0"));
            iccColor.append(lab.attribute("b", "0.0"));
        }
        else {
            for (int i = 0; i < channelValues.size(); i++) {
                int location = KoChannelInfo::displayPositionToChannelIndex(i, colorSpace()->channels());
                if (i != int(colorSpace()->alphaPos())) {
                    iccColor.append(pkNumberToString(double(channelValues.at(location)), 10));
                }
            }
        }
        colorDefinitions.append(PkString("icc-color(%1)").arg(iccColor.join(", ")));
        if (!profileList->contains(csName) && !sRGB) {
            profileList->insert(csName, colorSpace()->profile());
        }
    }

    return colorDefinitions.join(" ");
}

KoColor KoColor::fromSVG11(const PkString value, PkHash<PkString, const KoColorProfile *> profileList, KoColor current)
{
    KoColor parsed(KoColorSpaceRegistry::instance()->rgb16(KoColorSpaceRegistry::instance()->p709SRGBProfile()));
    parsed.setOpacity(1.0);

    if (value.toLower() == "none") {
        return parsed;
    }

    // add the sRGB default name.
    profileList.insert("sRGB", KoColorSpaceRegistry::instance()->p709SRGBProfile());
    // first, try to split at \w\d\) space.
    // we want to split up a string like... colorcolor none rgb(0.8, 0.1, 200%) #ff0000 icc-color(blah, 0.0, 1.0, 1.0, 0.0);
    // The original Qt regex "(#?\\w+|[\\w\\-]*\\(.+\\))\\s" was used with globalMatch:
    // each match is a token plus ONE trailing whitespace char. The hand-written
    // scanner below replicates it (ASCII word chars; greedy "(.+)" ends at the last
    // ')' that is followed by whitespace and has non-newline content after the '(').
    PkStringList colorDefinitions;
    PkString valueAdjust = value.split(u';').front();
    valueAdjust += PkString(" ");
    int pos = 0;

    const auto isWordChar = [](char16_t c) -> bool {
        return (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z')
                || (c >= u'0' && c <= u'9') || c == u'_';
    };
    const auto isSpace = [](char16_t c) -> bool {
        return c == u' ' || c == u'\t' || c == u'\n' || c == u'\r'
                || c == u'\f' || c == u'\v';
    };

    const int n = valueAdjust.size();
    int searchPos = 0;
    while (searchPos < n) {
        int matchEnd = -1;
        PkString token;
        for (int p = searchPos; p < n; ++p) {
            // alternative 1: #? \w+ \s
            int q = p;
            if (q < n && valueAdjust.at(q) == u'#') {
                ++q;
            }
            const int wordStart = q;
            while (q < n && isWordChar(valueAdjust.at(q))) {
                ++q;
            }
            if (q > wordStart && q < n && isSpace(valueAdjust.at(q))) {
                token = valueAdjust.mid(wordStart, q - wordStart);
                matchEnd = q + 1;
                break;
            }
            // alternative 2: [\w\-]* \( .+ \) \s
            q = p;
            while (q < n && (isWordChar(valueAdjust.at(q)) || valueAdjust.at(q) == u'-')) {
                ++q;
            }
            if (q < n && valueAdjust.at(q) == u'(') {
                for (int r = n - 1; r > q + 1; --r) {
                    if (valueAdjust.at(r) == u')' && r + 1 < n && isSpace(valueAdjust.at(r + 1))) {
                        bool hasContent = false;
                        for (int s = q + 1; s < r; ++s) {
                            if (valueAdjust.at(s) == u'\n') {
                                hasContent = false;
                                break;
                            }
                            hasContent = true;
                        }
                        if (hasContent) {
                            token = valueAdjust.mid(p, r + 1 - p);
                            matchEnd = r + 2;
                            break;
                        }
                    }
                }
                if (matchEnd > 0) {
                    break;
                }
            }
        }
        if (matchEnd < 0) {
            break;
        }
        colorDefinitions.append(token.trimmed());
        pos = matchEnd;
        searchPos = matchEnd;
    }

    if (pos < value.size()) {
        PkString remainder = value.right(value.size() - pos);
        remainder = pkStringReplaceAll(remainder, PkString(";"), PkString(), PkCaseSensitive);
        colorDefinitions.append(remainder);
    }
    dbgPigment << "Color definitions found during svg11parsing" << colorDefinitions.join(" ");

    for (PkString def : colorDefinitions) {
        if (def.toLower() == "currentcolor") {
            parsed = current;
        } else {
            PkColor defColor(def);
            if (defColor.isValid()) {
                parsed.fromQColor(defColor);
            } else if (def.toLower().startsWith("rgb")) {
                PkString parse = def.trimmed();
                std::vector<PkString> colors = parse.split(u',');
                PkString r = colors[0].right(colors[0].size() - 4).trimmed();
                PkString g = colors[1].trimmed();
                PkString b = colors[2].left(colors[2].size() - 1).trimmed();

                if (r.contains(PkString("%"))) {
                    r = r.left(r.size() - 1);
                    r = PkString("%1").arg(int((double(255 * r.toDouble()) / 100.0)));
                }

                if (g.contains(PkString("%"))) {
                    g = g.left(g.size() - 1);
                    g = PkString("%1").arg(int((double(255 * g.toDouble()) / 100.0)));
                }

                if (b.contains(PkString("%"))) {
                    b = b.left(b.size() - 1);
                    b = PkString("%1").arg(int((double(255 * b.toDouble()) / 100.0)));
                }
                parsed.fromQColor(PkColor(r.toInt(), g.toInt(), b.toInt()));

            } else if (def.toLower().startsWith("icc-color")) {
                std::vector<PkString> values = def.split(u',');
                PkString iccprofilename = values.front().split(u'(').back();
                values.erase(values.begin());

                // svg11 docs say that searching the name should be case-insensitive.
                PkStringList entry = PkStringList(profileList.keys()).filter(iccprofilename, PkCaseInsensitive);
                if (entry.empty()) {
                    continue;
                }
                const KoColorProfile *profile = profileList.value(entry.first());
                if (!profile) {
                    continue;
                }
                PkString colormodel = profile->colorModelID();
                PkString depth = "F32";
                if (colormodel == LABAColorModelID.id()) {
                    // let our xml handling deal with lab
                    PkVector<float> labV(3);
                    for (int i = 0; i < int(values.size()); i++) {
                        if (i < labV.size()) {
                            PkString entryValue = values.at(i);
                            entryValue = entryValue.split(u')').front();
                            labV[i] = float(entryValue.toDouble());
                        }
                    }
                    PkString lab = PkString("<Lab space='%1' L='%2' a='%3' b='%4' />")
                            .arg(profile->name())
                            .arg(double(labV[0]))
                            .arg(double(labV[1]))
                            .arg(double(labV[2]));
                    PkXmlDocument doc;
                    doc.setContent(lab);
                    parsed = KoColor::fromXML(doc.documentElement(), "U16");
                    continue;
                } else if (colormodel == CMYKAColorModelID.id()) {
                    depth = "U16";
                } else if (colormodel == XYZAColorModelID.id()) {
                    // Inkscape decided to have X and Z go from 0 to 2, and I can't for the live of me figure out why.
                    // So we're just not parsing XYZ.
                    continue;
                }
                const KoColorSpace * cs = KoColorSpaceRegistry::instance()->colorSpace(colormodel, depth, profile);
                if (!cs) {
                    continue;
                }
                parsed = KoColor(cs);
                PkVector<float> channelValues(parsed.colorSpace()->channelCount());
                channelValues.fill(0.0);
                channelValues[parsed.colorSpace()->alphaPos()] = 1.0;
                for (int channel = 0; channel < int(values.size()); channel++) {
                    int location = KoChannelInfo::displayPositionToChannelIndex(channel, parsed.colorSpace()->channels());
                    PkString entryValue = values.at(channel);
                    entryValue = entryValue.split(u')').front();
                    channelValues[location] = float(entryValue.toDouble());
                }
                parsed.colorSpace()->fromNormalisedChannelsValue(parsed.data(), channelValues);
            }
        }
    }

    return parsed;
}

PkString KoColor::toQString(const KoColor &color)
{
    PkStringList ls;
    for (KoChannelInfo *channel : KoChannelInfo::displayOrderSorted(color.colorSpace()->channels())) {
        int realIndex = KoChannelInfo::displayPositionToChannelIndex(channel->displayPosition(), color.colorSpace()->channels());
        ls << channel->name();
        ls << color.colorSpace()->channelValueText(color.data(), realIndex);
    }
    return ls.join(" ");
}

void KoColor::addMetadata(PkString key, PkVariant value)
{
    m_metadata.insert(key, value);
}

PkMap<PkString, PkVariant> KoColor::metadata() const
{
    return m_metadata;
}

void KoColor::clearMetadata()
{
    m_metadata.clear();
}

KoColor KoColor::createTransparent(const KoColorSpace *cs)
{
    KoColor result;

    result.m_colorSpace = KoColorSpaceRegistry::instance()->permanentColorspace(cs);
    result.m_size = cs->pixelSize();
    cs->transparentColor(result.m_data, 1);

    return result;
}

KRITAPIGMENT_EXPORT PkDebug operator<<(PkDebug dbg, const KoColor &color)
{
    dbg.nospace() << "KoColor (" << color.colorSpace()->id();

    const PkList<KoChannelInfo*> channels = color.colorSpace()->channels();
    for (auto it = channels.constBegin(); it != channels.constEnd(); ++it) {

        KoChannelInfo *ch = (*it);

        dbg.nospace() << ", " << ch->name() << ":";

        switch (ch->channelValueType()) {
        case KoChannelInfo::UINT8: {
            const quint8 *ptr = reinterpret_cast<const quint8*>(color.data() + ch->pos());
            dbg.nospace() << static_cast<int>(*ptr);
            break;
        } case KoChannelInfo::UINT16: {
            const quint16 *ptr = reinterpret_cast<const quint16*>(color.data() + ch->pos());
            dbg.nospace() << *ptr;
            break;
        } case KoChannelInfo::UINT32: {
            const quint32 *ptr = reinterpret_cast<const quint32*>(color.data() + ch->pos());
            dbg.nospace() << *ptr;
            break;
        } case KoChannelInfo::FLOAT16: {

#ifdef HAVE_OPENEXR
            const half *ptr = reinterpret_cast<const half*>(color.data() + ch->pos());
            dbg.nospace() << *ptr;
#else
            const quint16 *ptr = reinterpret_cast<const quint16*>(color.data() + ch->pos());
            dbg.nospace() << "UNSUPPORTED_F16(" << *ptr << ")";
#endif
            break;
        } case KoChannelInfo::FLOAT32: {
            const float *ptr = reinterpret_cast<const float*>(color.data() + ch->pos());
            dbg.nospace() << *ptr;
            break;
        } case KoChannelInfo::FLOAT64: {
            const double *ptr = reinterpret_cast<const double*>(color.data() + ch->pos());
            dbg.nospace() << *ptr;
            break;
        } case KoChannelInfo::INT8: {
            const qint8 *ptr = reinterpret_cast<const qint8*>(color.data() + ch->pos());
            dbg.nospace() << static_cast<int>(*ptr);
            break;
        } case KoChannelInfo::INT16: {
            const qint16 *ptr = reinterpret_cast<const qint16*>(color.data() + ch->pos());
            dbg.nospace() << *ptr;
            break;
        } case KoChannelInfo::OTHER: {
            const quint8 *ptr = reinterpret_cast<const quint8*>(color.data() + ch->pos());
            dbg.nospace() << "undef(" << static_cast<int>(*ptr) << ")";
            break;
        }
        }
    }
    dbg.nospace() << ")";
    return dbg.space();
}
