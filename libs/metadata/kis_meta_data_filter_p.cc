/*
 *  SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "kis_meta_data_filter_p.h"

#include <PkDateTime.h>
#include <PkString.h>
#include <PkVariant.h>

#include <KritaVersionWrapper.h>

#include "kis_meta_data_entry.h"
#include "kis_meta_data_schema.h"
#include "kis_meta_data_schema_registry.h"
#include "kis_meta_data_store.h"
#include "kis_meta_data_value.h"

#include "kis_debug.h"

using namespace KisMetaData;

AnonymizerFilter::~AnonymizerFilter()
{
}

bool AnonymizerFilter::defaultEnabled() const
{
    return false;
}

PkString AnonymizerFilter::id() const
{
    return "Anonymizer";
}

PkString AnonymizerFilter::name() const
{
    return PkString("Anonymizer");
}

PkString AnonymizerFilter::description() const
{
    return PkString("Remove personal information: author, location...");
}

void AnonymizerFilter::filter(KisMetaData::Store* store) const
{
    dbgMetaData << "Anonymize a store";
    const KisMetaData::Schema* dcSchema = KisMetaData::SchemaRegistry::instance()->schemaFromUri(KisMetaData::Schema::DublinCoreSchemaUri);
    store->removeEntry(dcSchema, "contributor");
    store->removeEntry(dcSchema, "creator");
    store->removeEntry(dcSchema, "publisher");
    store->removeEntry(dcSchema, "rights");

    const KisMetaData::Schema* psSchema = KisMetaData::SchemaRegistry::instance()->schemaFromUri(KisMetaData::Schema::PhotoshopSchemaUri);
    store->removeEntry(psSchema, "AuthorsPosition");
    store->removeEntry(psSchema, "CaptionWriter");
    store->removeEntry(psSchema, "Credit");
    store->removeEntry(psSchema, "City");
    store->removeEntry(psSchema, "Country");
}

//------------------------------------//
//---------- ToolInfoFilter ----------//
//------------------------------------//

ToolInfoFilter::~ToolInfoFilter()
{
}

bool ToolInfoFilter::defaultEnabled() const
{
    return true;
}

PkString ToolInfoFilter::id() const
{
    return "ToolInfo";
}

PkString ToolInfoFilter::name() const
{
    return PkString("Tool information");
}

PkString ToolInfoFilter::description() const
{
    return PkString("Add the name of the tool used for creation and the modification date");
}

void ToolInfoFilter::filter(KisMetaData::Store* store) const
{
    const KisMetaData::Schema* xmpSchema = KisMetaData::SchemaRegistry::instance()->schemaFromUri(KisMetaData::Schema::XMPSchemaUri);
    store->getEntry(xmpSchema, "ModifyDate").value() = Value(PkDate::currentDate());
    store->getEntry(xmpSchema, "MetadataDate").value() = Value(PkDate::currentDate());
    if (!store->containsEntry(xmpSchema, "CreatorTool")) {
        store->getEntry(xmpSchema, "CreatorTool").value() =
            Value(PkString("Krita %1").arg(KritaVersionWrapper::versionString()));
    }
}
