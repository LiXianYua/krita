/*
 *  SPDX-FileCopyrightText: 2009 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "kis_meta_data_type_info.h"

#include <PkHash.h>
#include <PkList.h>
#include <PkString.h>

struct KRITAMETADATA_EXPORT KisMetaData::TypeInfo::Private {
    Private() : embeddedTypeInfo(0), structureSchema(0), parser(0) {}
    PropertyType propertyType { KisMetaData::TypeInfo::BooleanType };
    const TypeInfo* embeddedTypeInfo;
    PkList< Choice> choices;
    Schema* structureSchema;
    PkString structureName;
    const Parser* parser;
private:
    static PkHash< const TypeInfo*, const TypeInfo*> orderedArrays;
    static PkHash< const TypeInfo*, const TypeInfo*> unorderedArrays;
    static PkHash< const TypeInfo*, const TypeInfo*> alternativeArrays;
public:
    static const TypeInfo* Boolean;
    static const TypeInfo* Integer;
    static const TypeInfo* Date;
    static const TypeInfo* Text;
    static const TypeInfo* Rational;
    static const TypeInfo* GPSCoordinate;
    static const TypeInfo* orderedArray(const TypeInfo*);
    static const TypeInfo* unorderedArray(const TypeInfo*);
    static const TypeInfo* alternativeArray(const TypeInfo*);
    static const TypeInfo* createChoice(PropertyType _propertiesType, const TypeInfo* _embedded, const PkList< Choice >&);
    static const TypeInfo* createStructure(Schema* _structureSchema, const PkString& name);
    static const TypeInfo* LangArray;
};
