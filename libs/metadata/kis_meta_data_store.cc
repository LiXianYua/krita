/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "kis_meta_data_store.h"

#include <PkStringList.h>
#include <PkStringHash.h>

#include <kis_debug.h>

#include "kis_meta_data_entry.h"
#include "kis_meta_data_filter.h"
#include "kis_meta_data_schema.h"
#include "kis_meta_data_schema_registry.h"
#include "kis_meta_data_value.h"

using namespace KisMetaData;

uint qHash(const Entry& e)
{
    return qHash(e.qualifiedName());
}

struct Store::Private {
    PkHash<PkString, Entry> entries;
};

Store::Store() : d(new Private)
{

}

Store::Store(const Store& s) : d(new Private(*s.d))
{
    // TODO: reaffect all schemas
}

Store::~Store()
{
    delete d;
}

void Store::copyFrom(const Store* store)
{
    for (PkHash<PkString, Entry>::const_iterator entryIt = store->begin();
            entryIt != store->end(); ++entryIt) {
        const Entry& entry = entryIt.value();
        if (entry.value().type() != KisMetaData::Value::Invalid) {
            if (containsEntry(entry.qualifiedName())) {
                getEntry(entry.qualifiedName()).value() = entry.value();
            } else {
                addEntry(entry);
            }
        }
    }
}

bool Store::addEntry(const Entry& entry)
{
    Q_ASSERT(!entry.name().isEmpty());
    if (d->entries.contains(entry.qualifiedName()) && d->entries[entry.qualifiedName()].isValid()) {
        dbgMetaData << "Entry" << entry.qualifiedName() << " already exists in the store, cannot be included twice";
        return false;
    }
    d->entries.insert(entry.qualifiedName(), entry);
    return true;
}

bool Store::empty() const
{
    return d->entries.isEmpty();
}

bool Store::isEmpty() const
{
    return d->entries.isEmpty();
}

bool Store::containsEntry(const PkString & entryKey) const
{
    return d->entries.contains(entryKey);
}

bool Store::containsEntry(const PkString & uri, const PkString & entryName) const
{
    const Schema* schema = SchemaRegistry::instance()->schemaFromUri(uri);
    return containsEntry(schema->generateQualifiedName(entryName));
}

bool Store::containsEntry(const KisMetaData::Schema* schema, const PkString & entryName) const
{
    if (schema) {
        return containsEntry(schema->generateQualifiedName(entryName));
    }
    return false;
}

Entry& Store::getEntry(const PkString & entryKey)
{
    if (!d->entries.contains(entryKey)) {
        PkStringList splitKey;
        for (const PkString& part : entryKey.split(u':')) {
            splitKey.append(part);
        }
        PkString prefix = splitKey[0];
        splitKey.pop_front();
        d->entries[entryKey] = Entry(SchemaRegistry::instance()->schemaFromPrefix(prefix),
                                     splitKey.join(":"),
                                     Value());
    }
    return d->entries [entryKey];
}

Entry& Store::getEntry(const PkString & uri, const PkString & entryName)
{
    const Schema* schema = SchemaRegistry::instance()->schemaFromUri(uri);
    Q_ASSERT(schema);
    return getEntry(schema, entryName);
}

Entry& Store::getEntry(const KisMetaData::Schema* schema, const PkString & entryName)
{
    return getEntry(schema->generateQualifiedName(entryName));
}


const Entry& Store::getEntry(const PkString & entryKey) const
{
    return d->entries[entryKey];
}

const Entry& Store::getEntry(const PkString & uri, const PkString & entryName) const
{
    const Schema* schema = SchemaRegistry::instance()->schemaFromUri(uri);
    Q_ASSERT(schema);
    return getEntry(schema, entryName);
}

const Entry& Store::getEntry(const KisMetaData::Schema* schema, const PkString & entryName) const
{
    return getEntry(schema->generateQualifiedName(entryName));
}

void Store::removeEntry(const PkString & entryKey)
{
    d->entries.remove(entryKey);
}

void Store::removeEntry(const PkString & uri, const PkString & entryName)
{
    const Schema* schema = SchemaRegistry::instance()->schemaFromUri(uri);
    Q_ASSERT(schema);
    removeEntry(schema, entryName);
}

void Store::removeEntry(const KisMetaData::Schema* schema, const PkString & entryName)
{
    removeEntry(schema->generateQualifiedName(entryName));
}

const Value& Store::getValue(const PkString & uri, const PkString & entryName) const
{
    return getEntry(uri, entryName).value();
}

PkHash<PkString, Entry>::const_iterator Store::begin() const
{
    return d->entries.constBegin();
}

PkHash<PkString, Entry>::const_iterator Store::end() const
{
    return d->entries.constEnd();
}

void Store::debugDump() const
{
    dbgMetaData << "=== Dumping MetaData Store ===";
    dbgMetaData << " - Metadata (there are" << d->entries.size() << " entries)";
    for (const Entry& e : d->entries.values()) {
        if (e.isValid()) {
            dbgMetaData << e;
        } else {
            dbgMetaData << "Invalid entry";
        }
    }
}

void Store::applyFilters(const PkList<const Filter*> & filters)
{
    dbgMetaData << "Apply " << filters.size() << " filters";
    for (const Filter* filter : filters) {
        filter->filter(this);
    }
}

PkList<PkString> Store::keys() const
{
    return d->entries.keys();
}

PkList<Entry> Store::entries() const
{
    return d->entries.values();
}
