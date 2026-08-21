/*
 *  SPDX-FileCopyrightText: 2006 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2006-2008 Thomas Zander <zander@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KoProperties.h"

#include <PkXmlDocument.h>
#include <PkDataStream.h>

#include <PkStream.h>

#include <cstdint>

namespace {

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
        if (c == '=') break;            // padding 终止，对齐 Qt
        const int v = pkBase64Value(c);
        if (v < 0) break;               // Qt 在首个非法字符处停止
        buf = (buf << 6) | static_cast<unsigned int>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<unsigned char>((buf >> bits) & 0xff));
        }
    }
    return PkByteArray(out);
}

} // namespace

class KoProperties::Private
{
public:
    PkMap<PkString, PkVariant> properties;
};

KoProperties::KoProperties()
        : d(new Private())
{
}

KoProperties::KoProperties(const KoProperties & rhs)
        : d(new Private())
{
    d->properties = rhs.d->properties;
}

KoProperties::~KoProperties()
{
    delete d;
}

PkMapIterator<PkString, PkVariant> KoProperties::propertyIterator() const
{
    return PkMapIterator<PkString, PkVariant>(d->properties);
}

bool KoProperties::isEmpty() const
{
    return d->properties.isEmpty();
}

void  KoProperties::load(const PkXmlElement &root)
{
    d->properties.clear();

    PkXmlElement e = root;
    PkXmlNode n = e.firstChild();

    while (!n.isNull()) {
        // We don't nest elements.
        PkXmlElement e = n.toElement();
        if (!e.isNull()) {
            if (e.tagName() == "property") {
                const PkString name = e.attribute("name");
                const PkString type = e.attribute("type");
                const PkString value = e.text();
                PkDataStream in(pkBase64Decode(value.PkToUtf8()));
                PkVariant v;
                in >> v;
                d->properties[name] = v;
            }
        }
        n = n.nextSibling();
    }
}

bool KoProperties::load(const PkString & s)
{
    PkXmlDocument doc;

    if (!doc.setContent(s))
        return false;
    load(doc.documentElement());

    return true;
}

void KoProperties::save(PkXmlElement &root) const
{
    PkXmlDocument doc = root.ownerDocument();
    PkMap<PkString, PkVariant>::Iterator it;
    for (it = d->properties.begin(); it != d->properties.end(); ++it) {
        PkXmlElement e = doc.createElement("property");
        e.setAttribute("name", it.key());
        PkVariant v = it.value();
        e.setAttribute("type", v.typeName());

        PkByteArray bytes;
        PkDataStream out(&bytes, PkStream::WriteOnly);
        out << v;
        const std::string b64 = pkBase64Encode(bytes);
        PkXmlText text = doc.createCDATASection(PkString(b64.c_str()));
        e.appendChild(text);
        root.appendChild(e);
    }
}

PkString KoProperties::store(const PkString &s) const
{
    PkXmlDocument doc = PkXmlDocument(s);
    PkXmlElement root = doc.createElement(s);
    doc.appendChild(root);

    save(root);
    return doc.toString();
}

void KoProperties::setProperty(const PkString & name, const PkVariant & value)
{
    // If there's an existing value for this name already, replace it.
    d->properties.insert(name, value);
}

bool KoProperties::property(const PkString & name, PkVariant & value) const
{
    PkMap<PkString, PkVariant>::const_iterator it = d->properties.constFind(name);
    if (it == d->properties.constEnd()) {
        return false;
    } else {
        value = *it;
        return true;
    }
}

PkVariant KoProperties::property(const PkString & name) const
{
    return d->properties.value(name, PkVariant());
}

int KoProperties::intProperty(const PkString & name, int def) const
{
    const PkVariant v = property(name);
    if (v.isValid())
        return v.toInt();
    else
        return def;

}

qreal KoProperties::doubleProperty(const PkString & name, qreal def) const
{
    const PkVariant v = property(name);
    if (v.isValid())
        return v.toDouble();
    else
        return def;
}

bool KoProperties::boolProperty(const PkString & name, bool def) const
{
    const PkVariant v = property(name);
    if (v.isValid())
        return v.toBool();
    else
        return def;
}

PkString KoProperties::stringProperty(const PkString & name, const PkString & def) const
{
    const PkVariant v = property(name);
    if (v.isValid())
        return v.toString();
    else
        return def;
}

bool KoProperties::contains(const PkString & key) const
{
    return d->properties.contains(key);
}

PkVariant KoProperties::value(const PkString & key) const
{
    return d->properties.value(key);
}

bool KoProperties::operator==(const KoProperties &other) const
{
    if (d->properties.count() != other.d->properties.count())
        return false;
    for (const PkString &key : d->properties.keys()) {
        if (other.d->properties.value(key) != d->properties.value(key))
            return false;
    }
    return true;
}
