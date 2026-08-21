/*
 *  SPDX-FileCopyrightText: 2007, 2009 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "kis_meta_data_schema.h"

#include <PkString.h>
#include <PkXmlDocument.h>
#include <PkXmlElement.h>
#include <PkVariant.h>

#include <fstream>
#include <iterator>
#include <sstream>

#include <PkStringHash.h>

#include "kis_meta_data_type_info_p.h"
#include "kis_meta_data_schema_p.h"
#include "kis_meta_data_value.h"

using namespace KisMetaData;

const PkString Schema::TIFFSchemaUri = "http://ns.adobe.com/tiff/1.0/";
const PkString Schema::EXIFSchemaUri = "http://ns.adobe.com/exif/1.0/";
const PkString Schema::DublinCoreSchemaUri = "http://purl.org/dc/elements/1.1/";
const PkString Schema::XMPSchemaUri = "http://ns.adobe.com/xap/1.0/";
const PkString Schema::XMPRightsSchemaUri = "http://ns.adobe.com/xap/1.0/rights/";
const PkString Schema::XMPMediaManagementUri = "http://ns.adobe.com/xap/1.0/sType/ResourceRef#";
const PkString Schema::MakerNoteSchemaUri = "http://www.calligra.org/krita/xmp/MakerNote/1.0/";
const PkString Schema::IPTCSchemaUri = "http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/";
const PkString Schema::PhotoshopSchemaUri = "http://ns.adobe.com/photoshop/1.0/";

bool Schema::Private::load(const PkString& _fileName)
{
    dbgMetaData << "Loading from " << _fileName;

    std::ifstream file(_fileName.PkToUtf8(), std::ios::binary);
    if (!file) {
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    PkXmlDocument document;
    PkString error;
    int line = 0, column = 0;
    if (!document.setContent(PkString(content.c_str()), &error, &line, &column)) {
        dbgMetaData << error << " at " << line << ", " << column << " in " << _fileName;
        return false;
    }

    PkXmlElement docElem = document.documentElement();
    if (docElem.tagName() != "schema") {
        dbgMetaData << _fileName << ": invalid root name";
        return false;
    }

    if (!docElem.hasAttribute("prefix")) {
        dbgMetaData << _fileName << ": missing prefix.";
        return false;
    }

    if (!docElem.hasAttribute("uri")) {
        dbgMetaData << _fileName << ": missing uri.";
        return false;
    }

    prefix = docElem.attribute("prefix");
    uri = docElem.attribute("uri");
    dbgMetaData << ppVar(prefix) << ppVar(uri);

    PkXmlElement structuresElt = docElem.firstChildElement("structures");
    if (structuresElt.isNull()) {
        return false;
    }

    PkXmlElement propertiesElt = docElem.firstChildElement("properties");
    if (propertiesElt.isNull()) {
        return false;
    }

    parseStructures(structuresElt);
    parseProperties(propertiesElt);

    return true;
}

void Schema::Private::parseStructures(PkXmlElement& elt)
{
    Q_ASSERT(elt.tagName() == "structures");
    dbgMetaData << "Parse structures";

    PkXmlElement e = elt.firstChildElement();
    for (; !e.isNull(); e = e.nextSiblingElement()) {
        if (e.tagName() == "structure") {
            parseStructure(e);
        } else {
            errMetaData << "Invalid tag: " << e.tagName() << " in structures section";
        }
    }
}

void Schema::Private::parseStructure(PkXmlElement& elt)
{
    Q_ASSERT(elt.tagName() == "structure");

    if (!elt.hasAttribute("name")) {
        errMetaData << "Name is required for a structure";
        return;
    }

    PkString structureName = elt.attribute("name");
    if (structures.contains(structureName)) {
        errMetaData << structureName << " is defined twice";
        return;
    }
    dbgMetaData << "Parsing structure " << structureName;

    if (!elt.hasAttribute("prefix")) {
        errMetaData << "prefix is required for structure " << structureName;
        return;
    }

    if (!elt.hasAttribute("uri")) {
        errMetaData << "uri is required for structure " << structureName;
        return;
    }

    PkString structurePrefix = elt.attribute("prefix");
    PkString structureUri = elt.attribute("uri");
    dbgMetaData << ppVar(structurePrefix) << ppVar(structureUri);

    Schema* schema = new Schema(structureUri, structurePrefix);
    PkXmlElement e;
    for (e = elt.firstChildElement(); !e.isNull(); e = e.nextSiblingElement()) {
        EntryInfo info;
        PkString name;

        if (!parseEltType(e, info, name, false, false)) {
            continue;
        }

        if (schema->d->types.contains(name)) {
            errMetaData << structureName << " already contains a field " << name;
            continue;
        }

        schema->d->types[ name ] = info;
    }

    structures[ structureName ] = TypeInfo::Private::createStructure(schema, structureName);
}

void Schema::Private::parseProperties(PkXmlElement& elt)
{
    Q_ASSERT(elt.tagName() == "properties");
    dbgMetaData << "Parse properties";

    PkXmlElement e;
    for (e = elt.firstChildElement(); !e.isNull(); e = e.nextSiblingElement()) {
        EntryInfo info;
        PkString name;

        if (!parseEltType(e, info, name, false, false)) {
            continue;
        }

        if (types.contains(name)) {
            errMetaData << name << " already defined.";
            continue;
        }

        types[ name ] = info;
    }
}

bool Schema::Private::parseEltType(PkXmlElement &elt,
                                   EntryInfo &entryInfo,
                                   PkString &name,
                                   bool ignoreStructure,
                                   bool ignoreName)
{
    dbgMetaData << elt.tagName() << name << ignoreStructure << ignoreName;

    PkString tagName = elt.tagName();
    if (!ignoreName && !elt.hasAttribute("name")) {
        errMetaData << "Missing name attribute for tag " << tagName;
        return false;
    }
    name = elt.attribute("name");

    // TODO parse qualifier
    if (tagName == "integer") {
        entryInfo.propertyType = TypeInfo::Private::Integer;
    } else if (tagName == "boolean") {
        entryInfo.propertyType = TypeInfo::Private::Boolean;
    } else if (tagName == "date") {
        entryInfo.propertyType = TypeInfo::Private::Date;
    } else if (tagName == "text") {
        entryInfo.propertyType = TypeInfo::Private::Text;
    } else if (tagName == "seq") {
        const TypeInfo* ei = parseAttType(elt, ignoreStructure);
        if (!ei) {
            ei = parseEmbType(elt, ignoreStructure);
        }

        if (!ei) {
            errMetaData << "No type defined for " << name;
            return false;
        }

        entryInfo.propertyType = TypeInfo::Private::orderedArray(ei);
    } else if (tagName == "bag") {
        const TypeInfo* ei = parseAttType(elt, ignoreStructure);
        if (!ei) {
            ei = parseEmbType(elt, ignoreStructure);
        }

        if (!ei) {
            errMetaData << "No type defined for " << name;
            return false;
        }

        entryInfo.propertyType = TypeInfo::Private::unorderedArray(ei);
    } else if (tagName == "alt") {
        const TypeInfo* ei = parseAttType(elt, ignoreStructure);
        if (!ei) {
            ei = parseEmbType(elt, ignoreStructure);
        }

        if (!ei) {
            errMetaData << "No type defined for " << name;
            return false;
        }

        entryInfo.propertyType = TypeInfo::Private::alternativeArray(ei);
    } else if (tagName == "lang") {
        entryInfo.propertyType = TypeInfo::Private::LangArray;
    } else if (tagName == "rational") {
        entryInfo.propertyType = TypeInfo::Private::Rational;
    } else if (tagName == "gpscoordinate") {
        entryInfo.propertyType = TypeInfo::Private::GPSCoordinate;
    } else if (tagName == "openedchoice" || tagName == "closedchoice") {
        entryInfo.propertyType = parseChoice(elt);
    } else if (!ignoreStructure && structures.contains(tagName)) {
        entryInfo.propertyType = structures.value(tagName);
    } else {
        errMetaData << tagName << " isn't a type.";
        return false;
    }

    return true;
}

const TypeInfo* Schema::Private::parseAttType(PkXmlElement& elt, bool ignoreStructure)
{
    if (!elt.hasAttribute("type")) {
        return 0;
    }

    PkString type = elt.attribute("type");
    if (type == "integer") {
        return TypeInfo::Private::Integer;
    } else if (type == "boolean") {
        return TypeInfo::Private::Boolean;
    } else if (type == "date") {
        return TypeInfo::Private::Date;
    } else if (type == "text") {
        return TypeInfo::Private::Text;
    } else if (type == "rational") {
        return TypeInfo::Private::Rational;
    } else if (!ignoreStructure && structures.contains(type)) {
        return structures[type];
    }

    errMetaData << "Unsupported type: " << type << " in an attribute";
    return nullptr;
}

const TypeInfo* Schema::Private::parseEmbType(PkXmlElement& elt, bool ignoreStructure)
{
    dbgMetaData << "Parse embedded type for " << elt.tagName();

    PkXmlElement e;
    for (e = elt.firstChildElement(); !e.isNull(); e = e.nextSiblingElement()) {
        PkString type = e.tagName();
        if (type == "integer") {
            return TypeInfo::Private::Integer;
        } else if (type == "boolean") {
            return TypeInfo::Private::Boolean;
        } else if (type == "date") {
            return TypeInfo::Private::Date;
        } else if (type == "text") {
            return TypeInfo::Private::Text;
        } else if (type == "openedchoice" || type == "closedchoice") {
            return parseChoice(e);
        } else if (!ignoreStructure && structures.contains(type)) {
            return structures[type];
        }
    }

    return nullptr;
}

const TypeInfo* Schema::Private::parseChoice(PkXmlElement& elt)
{
    const TypeInfo* choiceType = parseAttType(elt, true);
    TypeInfo::PropertyType propertyType;
    if (elt.tagName() == "openedchoice") {
        propertyType = TypeInfo::OpenedChoice;
    } else {
        Q_ASSERT(elt.tagName() == "closedchoice");
        propertyType = TypeInfo::ClosedChoice;
    }

    PkXmlElement e;
    PkList<TypeInfo::Choice> choices;
    for (e = elt.firstChildElement(); !e.isNull(); e = e.nextSiblingElement()) {
        EntryInfo info;
        PkString name;

        if (!parseEltType(e, info, name, true, true)) {
            continue;
        }

        if (!choiceType) {
            choiceType = info.propertyType;
        }

        if (choiceType != info.propertyType) {
            errMetaData << "All members of a choice need to be of the same type";
            continue;
        }

        PkString text = e.text();
        PkVariant var = text;

        if (choiceType->propertyType() == TypeInfo::IntegerType) {
            var = PkVariant(var.toInt());
        } else if (choiceType->propertyType() == TypeInfo::DateType) {
            // TODO: the date parser isn't very good with XMP date
            // (it doesn't support YYYY and YYYY-MM)
            var = PkVariant(var.toDateTime());
        }
        choices.push_back(TypeInfo::Choice(Value(var), name));
    }

    return TypeInfo::Private::createChoice(propertyType, choiceType, choices);
}

Schema::Schema()
        : d(new Private)
{
}

Schema::Schema(const PkString & _uri, const PkString & _ns)
        : d(new Private)
{
    d->uri = _uri;
    d->prefix = _ns;
}

Schema::~Schema()
{
    dbgMetaData << "Deleting schema " << d->uri << " " << d->prefix;
    dbgMetaData.noquote() << kisBacktrace();
    delete d;
}

const TypeInfo* Schema::propertyType(const PkString& _propertyName) const
{
    if (d->types.contains(_propertyName)) {
        return d->types.value(_propertyName).propertyType;
    }
    return 0;
}

const TypeInfo* Schema::structure(const PkString& _structureName) const
{
    return d->structures.value(_structureName);
}


PkString Schema::uri() const
{
    return d->uri;
}

PkString Schema::prefix() const
{
    return d->prefix;
}

PkString Schema::generateQualifiedName(const PkString & name) const
{
    dbgMetaData << "generateQualifiedName for " << name;
    Q_ASSERT(!name.isEmpty());
    return prefix() + PkString(":") + name;
}

PkDebug operator<<(PkDebug debug, const KisMetaData::Schema &c)
{
    debug.nospace() << "Uri = " << c.uri() << " Prefix = " << c.prefix();
    return debug.space();
}
