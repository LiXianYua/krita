/*
 *  SPDX-FileCopyrightText: 2006 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2007, 2010 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_properties_configuration.h"


#include <kis_debug.h>

#include "kis_image.h"
#include "kis_transaction.h"
#include "kis_undo_adapter.h"
#include "kis_painter.h"
#include "kis_selection.h"
#include "KoID.h"
#include "kis_types.h"
#include <KoColor.h>
#include <KoColorModelStandardIds.h>
#include <KoColorSpaceRegistry.h>

#include <string>
#include <vector>

namespace {

// PkByteArray 缺 fromBase64/toBase64（R-31 拥有 wire codec，S 线先壳本地兜底）。
// 纯 ASCII 往返，语义对齐 base64 标准编码（含换行跳过、
// 末尾 '=' 填充）。
PkByteArray pkFromBase64(const PkString &text)
{
    static const signed char b64[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };
    const std::string in = text.PkToUtf8();
    std::vector<uint8_t> out;
    out.reserve((in.size() / 4) * 3);
    int val = 0;
    int valb = -8;
    for (unsigned char c : in) {
        if (b64[c] == -1) continue; // 跳过换行/填充等非 base64 字符
        val = (val << 6) + b64[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return PkByteArray(reinterpret_cast<const char *>(out.data()), static_cast<int>(out.size()));
}

PkString pkToBase64(const PkByteArray &data)
{
    static const char b64c[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const char *in = data.constData();
    const int n = data.size();
    std::string out;
    out.reserve(((n + 2) / 3) * 4);
    for (int i = 0; i < n; i += 3) {
        const uint32_t v = (static_cast<uint8_t>(in[i]) << 16)
            | ((i + 1 < n ? static_cast<uint8_t>(in[i + 1]) : 0) << 8)
            | (i + 2 < n ? static_cast<uint8_t>(in[i + 2]) : 0);
        out.push_back(b64c[(v >> 18) & 0x3F]);
        out.push_back(b64c[(v >> 12) & 0x3F]);
        out.push_back(i + 1 < n ? b64c[(v >> 6) & 0x3F] : '=');
        out.push_back(i + 2 < n ? b64c[v & 0x3F] : '=');
    }
    return PkString(out.c_str());
}

} // namespace

struct Q_DECL_HIDDEN KisPropertiesConfiguration::Private {
    PkMap<PkString, PkVariant> properties;
    PkSet<PkString> notSavedProperties;
};

KisPropertiesConfiguration::KisPropertiesConfiguration() : d(new Private)
{
}

KisPropertiesConfiguration::~KisPropertiesConfiguration()
{
    delete d;
}

KisPropertiesConfiguration::KisPropertiesConfiguration(const KisPropertiesConfiguration& rhs)
    : KisSerializableConfiguration(rhs)
    , d(new Private(*rhs.d))
{
}

KisPropertiesConfiguration &KisPropertiesConfiguration::operator=(const KisPropertiesConfiguration &rhs)
{
    if (&rhs != this) {
        *d = *rhs.d;
    }

    return *this;
}

bool KisPropertiesConfiguration::fromXML(const PkString & xml, bool clear)
{
    if (clear) {
        clearProperties();
    }

    PkXmlDocument doc;
    bool retval = bool(doc.setContent(xml));
    if (retval) {
        PkXmlElement e = doc.documentElement();
        fromXML(e);
    }
    return retval;
}

void KisPropertiesConfiguration::fromXML(const PkXmlElement &root)
{
    PkXmlElement e;
    for (e = root.firstChildElement("param"); !e.isNull(); e = e.nextSiblingElement("param")) {
        PkString name = e.attribute("name");
        PkString value = e.text();

        // Older versions didn't have a "type" parameter,
        // so fall back to the old behavior if it's missing.
        if (!e.hasAttribute("type")) {
            d->properties[name] = PkVariant(value);
        } else if (e.attribute("type") == "bytearray") {
            d->properties[name] = PkVariant(pkFromBase64(value));
        } else {
            d->properties[name] = value;
        }
    }
}

void KisPropertiesConfiguration::toXML(PkXmlDocument& doc, PkXmlElement& root) const
{
    PkMap<PkString, PkVariant>::ConstIterator it;
    for (it = d->properties.constBegin(); it != d->properties.constEnd(); ++it) {
        if (d->notSavedProperties.contains(it.key())) {
            continue;
        }

        PkXmlElement e = doc.createElement("param");
        e.setAttribute("name", it.key());
        PkString type = "string";
        PkVariant v = it.value();
        PkXmlText text;
        if (v.canConvert<KisCubicCurve>()) {
            text = doc.createCDATASection(v.value<KisCubicCurve>().toString());
        } else if (v.canConvert<KoColor>()) {
            PkXmlDocument cdataDoc = PkXmlDocument("color");
            PkXmlElement cdataRoot = cdataDoc.createElement("color");
            cdataDoc.appendChild(cdataRoot);
            v.value<KoColor>().toXML(cdataDoc, cdataRoot);
            text = cdataDoc.createCDATASection(cdataDoc.toString());
            type = "color";
        } else if(v.type() == PkVariant::String ) {
            text = doc.createCDATASection(v.toString());  // XXX: Unittest this!
            type = "string";
        } else if(v.type() == PkVariant::ByteArray ) {
            text = doc.createTextNode(pkToBase64(v.toByteArray())); // Arbitrary Data
            type = "bytearray";
        } else {
            text = doc.createTextNode(v.toString());
            type = "internal";
        }
        e.setAttribute("type", type);
        e.appendChild(text);
        root.appendChild(e);
    }
}

PkString KisPropertiesConfiguration::toXML() const
{
    PkXmlDocument doc = PkXmlDocument("params");
    PkXmlElement root = doc.createElement("params");
    doc.appendChild(root);
    toXML(doc, root);
    return doc.toString();
}


bool KisPropertiesConfiguration::hasProperty(const PkString& name) const
{
    return d->properties.contains(name);
}

void KisPropertiesConfiguration::setProperty(const PkString & name, const PkVariant & value)
{
    if (d->properties.find(name) == d->properties.end()) {
        d->properties.insert(name, value);
    } else {
        d->properties[name] = value;
    }
}

bool KisPropertiesConfiguration::getProperty(const PkString & name, PkVariant & value) const
{
    if (d->properties.constFind(name) == d->properties.constEnd()) {
        return false;
    } else {
        value = d->properties.value(name);
        return true;
    }
}

PkVariant KisPropertiesConfiguration::getProperty(const PkString & name) const
{
    return d->properties.value(name, PkVariant());
}


int KisPropertiesConfiguration::getInt(const PkString & name, int def) const
{
    PkVariant v = getProperty(name);
    if (v.isValid())
        return v.toInt();
    else
        return def;

}

double KisPropertiesConfiguration::getDouble(const PkString & name, double def) const
{
    PkVariant v = getProperty(name);
    if (v.isValid())
        return v.toDouble();
    else
        return def;
}

float KisPropertiesConfiguration::getFloat(const PkString & name, float def) const
{
    PkVariant v = getProperty(name);
    if (v.isValid())
        return (float)v.toDouble();
    else
        return def;
}


bool KisPropertiesConfiguration::getBool(const PkString & name, bool def) const
{
    PkVariant v = getProperty(name);
    if (v.isValid())
        return v.toBool();
    else
        return def;
}

PkString KisPropertiesConfiguration::getString(const PkString & name, const PkString & def) const
{
    PkVariant v = getProperty(name);
    if (v.isValid())
        return v.toString();
    else
        return def;
}

KisCubicCurve KisPropertiesConfiguration::getCubicCurve(const PkString & name, const KisCubicCurve & curve) const
{
    PkVariant v = getProperty(name);
    if (v.isValid()) {
        if (v.canConvert<KisCubicCurve>()) {
            return v.value<KisCubicCurve>();
        } else {
            return KisCubicCurve(v.toString());
        }
    } else
        return curve;
}

KoColor KisPropertiesConfiguration::getColor(const PkString& name, const KoColor& color) const
{
    PkVariant v = getProperty(name);

    if (v.isValid()) {
        switch(v.type()) {
        case PkVariant::UserType:
        {
            if (v.canConvert<KoColor>()) {
                return v.value<KoColor>();
            }
            if (v.canConvert<PkColor>()) {
                PkColor c = v.value<PkColor>();
                KoColor kc(c, KoColorSpaceRegistry::instance()->rgb8());
                return kc;
            }
            break;
        }
        case PkVariant::String:
        {
            PkXmlDocument doc;
            if (doc.setContent(v.toString())) {
                PkXmlElement e = doc.documentElement().firstChild().toElement();
                bool ok;
                KoColor c = KoColor::fromXML(e, Integer16BitsColorDepthID.id(), &ok);
                if (ok) {
                    return c;
                }
            }
            else {
                PkColor c(v.toString());
                if (c.isValid()) {
                    KoColor kc(c, KoColorSpaceRegistry::instance()->rgb8());
                    return kc;
                }
            }
            break;
        }
        case PkVariant::Int:
        {
            PkColor c;
            c.setRgba(static_cast<quint32>(v.toInt()));
            if (c.isValid()) {
                KoColor kc(c, KoColorSpaceRegistry::instance()->rgb8());
                return kc;
            }
            break;
        }
        default:
            ;
        }
    }
    return color;
}

void KisPropertiesConfiguration::dump() const
{
    PkMap<PkString, PkVariant>::ConstIterator it;
    for (it = d->properties.constBegin(); it != d->properties.constEnd(); ++it) {
        if (it->type() == PkVariant::ByteArray) {
            PkByteArray ba = it->toByteArray();

            if (ba.size() > 32) {
                qDebug() << it.key() << " = " << PkString("...skipped total %1 bytes...").arg(ba.size()) << it.value().typeName();
            } else {
                qDebug() << it.key() << " = " << it.value().toString() << it.value().typeName();
            }
        } else {
            qDebug() << it.key() << " = " << it.value().toString() << it.value().typeName();
        }
    }

}

void KisPropertiesConfiguration::clearProperties()
{
    d->properties.clear();
}

void KisPropertiesConfiguration::setPropertyNotSaved(const PkString& name)
{
    d->notSavedProperties.insert(name);
}

PkMap<PkString, PkVariant> KisPropertiesConfiguration::getProperties() const
{
    return d->properties;
}

void KisPropertiesConfiguration::removeProperty(const PkString & name)
{
    d->properties.remove(name);
}

PkList<PkString> KisPropertiesConfiguration::getPropertiesKeys() const
{
    return d->properties.keys();
}

void KisPropertiesConfiguration::getPrefixedProperties(const PkString &prefix, KisPropertiesConfiguration *config) const
{
    const int prefixSize = prefix.size();

    const PkList<PkString> keys = getPropertiesKeys();
    for (const PkString &key : keys) {
        if (key.startsWith(prefix)) {
            config->setProperty(key.mid(prefixSize), getProperty(key));
        }
    }

    PkString fullPrefix;
    const PkString parentPrefix = getString(extractedPrefixKey());
    if (!parentPrefix.isEmpty()) {
        fullPrefix = parentPrefix + "/" + prefix;
    } else {
        fullPrefix = prefix;
    }

    config->setProperty(extractedPrefixKey(), fullPrefix);
    config->setPropertyNotSaved(extractedPrefixKey());
}

void KisPropertiesConfiguration::getPrefixedProperties(const PkString &prefix, KisPropertiesConfigurationSP config) const
{
    getPrefixedProperties(prefix, config.data());
}

void KisPropertiesConfiguration::setPrefixedProperties(const PkString &prefix, const KisPropertiesConfiguration *config)
{
    const PkList<PkString> keys = config->getPropertiesKeys();
    for (const PkString &key : keys) {
        this->setProperty(prefix + key, config->getProperty(key));
    }
}

void KisPropertiesConfiguration::setPrefixedProperties(const PkString &prefix, const KisPropertiesConfigurationSP config)
{
    setPrefixedProperties(prefix, config.data());
}

PkString KisPropertiesConfiguration::extractedPrefixKey()
{
    static const PkString key = "__extractedFromPrefix";
    return key;
}

PkString KisPropertiesConfiguration::escapeString(const PkString &string)
{
    PkString result = pkStringReplaceAll(string, PkString(";"), PkString("\\;"), PkCaseSensitive);
    result = pkStringReplaceAll(result, PkString("]"), PkString("\\]"), PkCaseSensitive);
    result = pkStringReplaceAll(result, PkString(">"), PkString("\\>"), PkCaseSensitive);
    return result;
}

PkString KisPropertiesConfiguration::unescapeString(const PkString &string)
{
    PkString result = pkStringReplaceAll(string, PkString("\\;"), PkString(";"), PkCaseSensitive);
    result = pkStringReplaceAll(result, PkString("\\]"), PkString("]"), PkCaseSensitive);
    result = pkStringReplaceAll(result, PkString("\\>"), PkString(">"), PkCaseSensitive);
    return result;
}

void KisPropertiesConfiguration::setProperty(const PkString &name, const PkStringList &value)
{
    PkStringList escapedList;
    escapedList.reserve(value.size());

    for (const PkString &str : value) {
        escapedList << escapeString(str);
    }

    setProperty(name, escapedList.join(';'));
}

PkStringList KisPropertiesConfiguration::getStringList(const PkString &name, const PkStringList &defaultValue) const
{
    if (!hasProperty(name)) return defaultValue;

    const PkString joined = getString(name);

    PkStringList result;

    int afterLastMatch = -1;
    for (int i = 0; i < joined.size(); i++) {
        const bool lastChunk = i == joined.size() - 1;
        const bool matchedSplitter = joined[i] == ';' && (i == 0 || joined[i - 1] != '\\');

        if (lastChunk || matchedSplitter) {
            result << unescapeString(joined.mid(afterLastMatch, i - afterLastMatch + int(lastChunk && !matchedSplitter)));
            afterLastMatch = i + 1;
        }

        if (lastChunk && matchedSplitter) {
            result << PkString();
        }
    }

    return result;
}

PkStringList KisPropertiesConfiguration::getPropertyLazy(const PkString &name, const PkStringList &defaultValue) const
{
    return getStringList(name, defaultValue);
}

bool KisPropertiesConfiguration::compareTo(const KisPropertiesConfiguration* rhs) const
{
    if (rhs == nullptr)
        return false;

    for(const auto& propertyName: getPropertiesKeys()) {
        if (getProperty(propertyName) != rhs->getProperty(propertyName))
            return false;
    }

    return true;
}

// --- factory ---

struct Q_DECL_HIDDEN KisPropertiesConfigurationFactory::Private {
};

KisPropertiesConfigurationFactory::KisPropertiesConfigurationFactory() : d(new Private)
{
}

KisPropertiesConfigurationFactory::~KisPropertiesConfigurationFactory()
{
    delete d;
}

KisSerializableConfigurationSP KisPropertiesConfigurationFactory::createDefault()
{
    return new KisPropertiesConfiguration();
}

KisSerializableConfigurationSP KisPropertiesConfigurationFactory::create(const PkXmlElement& e)
{
    KisPropertiesConfigurationSP pc = new KisPropertiesConfiguration();
    pc->fromXML(e);
    return pc;
}

