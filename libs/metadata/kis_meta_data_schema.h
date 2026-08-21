/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _KIS_META_DATA_SCHEMA_H_
#define _KIS_META_DATA_SCHEMA_H_

#include <kritametadata_export.h>
#include <kis_debug.h>

class PkString;

namespace KisMetaData
{

class SchemaRegistry;
class TypeInfo;

class KRITAMETADATA_EXPORT Schema
{
    friend class SchemaRegistry;

public:

    virtual ~Schema();

    static const PkString TIFFSchemaUri;
    static const PkString EXIFSchemaUri;
    static const PkString DublinCoreSchemaUri;
    static const PkString XMPSchemaUri;
    static const PkString XMPRightsSchemaUri;
    static const PkString XMPMediaManagementUri;
    static const PkString MakerNoteSchemaUri;
    static const PkString IPTCSchemaUri;
    static const PkString PhotoshopSchemaUri;
private:
    Schema();
    Schema(const PkString & _uri, const PkString & _ns);
public:
    /**
     * @return the \ref TypeInfo associated with a given a property ( @p _propertyName ).
     */
    const TypeInfo* propertyType(const PkString& _propertyName) const;
    /**
     * @return the \ref TypeInfo describing a given structure of that schema
     */
    const TypeInfo* structure(const PkString& _structureName) const;
public:
    PkString uri() const;
    PkString prefix() const;
    PkString generateQualifiedName(const PkString &) const;
private:
    struct Private;
    Private* const d;
};

}

KRITAMETADATA_EXPORT PkDebug operator<<(PkDebug debug, const KisMetaData::Schema &c);

#endif
