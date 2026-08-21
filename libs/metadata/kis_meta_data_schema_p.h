/*
 *  SPDX-FileCopyrightText: 2007, 2009 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "kis_meta_data_schema.h"

#include <PkHash.h>
#include <PkString.h>

class PkXmlElement;

namespace KisMetaData
{
struct Schema::Private {
    PkString uri;
    PkString prefix;
    struct EntryInfo {
        const TypeInfo* propertyType;
        PkHash<PkString, TypeInfo*> qualifiers;
    };
    PkHash<PkString, EntryInfo> types;
    PkHash<PkString, const TypeInfo*> structures;
    bool load(const PkString&);
private:
    void parseStructures(PkXmlElement&);
    void parseStructure(PkXmlElement&);
    void parseProperties(PkXmlElement&);
    bool parseEltType(PkXmlElement&, EntryInfo& entryInfo, PkString& name, bool ignoreStructure, bool ignoreName);
    const TypeInfo* parseAttType(PkXmlElement&, bool ignoreStructure);
    const TypeInfo* parseEmbType(PkXmlElement&, bool ignoreStructure);
    const TypeInfo* parseChoice(PkXmlElement&);
};
}
