/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KisResourceModel.h"

#include <PkSqlQuery.h>

#include "KisResourceCacheDb.h"
#include "KisResourceLocator.h"
#include "KisResourceModelProvider.h"
#include "KisResourceQueryMapper.h"
#include "KisStorageModel.h"

struct KisAllResourcesModel::Private
{
    PkString resourceType;
    PkVector<KisResourceRecord> records;
    bool closed = false;
};

KisAllResourcesModel::KisAllResourcesModel(const PkString &resourceType,
                                           PkObject *parent)
    : PkObject(parent)
    , d(new Private)
{
    d->resourceType = resourceType;

    KisStorageModel *storageModel = KisStorageModel::instance();
    KisResourceLocator *locator = KisResourceLocator::instance();
    if (storageModel) {
        PkObject::connect(storageModel,
                          &KisStorageModel::storageEnabled,
                          this,
                          &KisAllResourcesModel::storageActiveStateChanged);
        PkObject::connect(storageModel,
                          &KisStorageModel::storageDisabled,
                          this,
                          &KisAllResourcesModel::storageActiveStateChanged);
        PkObject::connect(storageModel,
                          &KisStorageModel::storageResynchronized,
                          this,
                          &KisAllResourcesModel::storageResynchronized);
        PkObject::connect(storageModel,
                          &KisStorageModel::storagesBulkSynchronizationFinished,
                          this,
                          &KisAllResourcesModel::storagesBulkSynchronizationFinished);
    }
    if (locator) {
        PkObject::connect(locator,
                          &KisResourceLocator::beginExternalResourceImport,
                          this,
                          &KisAllResourcesModel::beginExternalResourceImport);
        PkObject::connect(locator,
                          &KisResourceLocator::endExternalResourceImport,
                          this,
                          &KisAllResourcesModel::endExternalResourceImport);
        PkObject::connect(locator,
                          &KisResourceLocator::beginExternalResourceRemove,
                          this,
                          &KisAllResourcesModel::beginExternalResourceRemove);
        PkObject::connect(locator,
                          &KisResourceLocator::endExternalResourceRemove,
                          this,
                          &KisAllResourcesModel::endExternalResourceRemove);
        PkObject::connect(locator,
                          &KisResourceLocator::resourceActiveStateChanged,
                          this,
                          &KisAllResourcesModel::slotResourceActiveStateChanged);
    }

    refresh();
}

KisAllResourcesModel::~KisAllResourcesModel()
{
    delete d;
}

PkVector<KisResourceRecord> KisAllResourcesModel::records() const
{
    return d->records;
}

PkVector<KoResourceSP> KisAllResourcesModel::resources() const
{
    PkVector<KoResourceSP> result;
    for (const KisResourceRecord &record : d->records) {
        KoResourceSP resource = resourceForId(record.id);
        if (resource) {
            result.append(resource);
        }
    }
    return result;
}

KoResourceSP KisAllResourcesModel::resourceForId(int id) const
{
    KisResourceLocator *locator = KisResourceLocator::instance();
    return locator && id >= 0 ? locator->resourceForId(id) : KoResourceSP();
}

bool KisAllResourcesModel::resourceExists(const PkString &md5,
                                          const PkString &filename,
                                          const PkString &name) const
{
    struct Candidate {
        const PkString *value;
        const char *column;
        const char *placeholder;
    };
    const Candidate candidates[] = {
        {&md5, "resources.md5sum", ":value"},
        {&filename, "resources.filename", ":value"},
        {&name, "resources.name", ":value"},
    };

    for (const Candidate &candidate : candidates) {
        if (candidate.value->isEmpty()) {
            continue;
        }
        const PkString sql =
            PkString("SELECT resources.id FROM resources ") +
            PkString("JOIN resource_types ON resource_types.id = resources.resource_type_id ") +
            PkString("WHERE resource_types.name = :resource_type AND ") +
            PkString(candidate.column) + PkString(" = ") +
            PkString(candidate.placeholder) + PkString(" LIMIT 1");
        PkSqlQuery query;
        if (!query.prepare(sql)) {
            continue;
        }
        query.bindValue(PkString(":resource_type"), PkVariant(d->resourceType));
        query.bindValue(PkString(":value"), PkVariant(*candidate.value));
        if (query.exec() && query.first()) {
            return true;
        }
    }
    return false;
}

PkVector<KoResourceSP> KisAllResourcesModel::resourcesForFilename(
    const PkString &filename) const
{
    PkVector<KoResourceSP> result;
    if (filename.isEmpty()) {
        return result;
    }

    PkSqlQuery query;
    if (!query.prepare(PkString(
            "SELECT resources.id FROM resources "
            "JOIN resource_types ON resources.resource_type_id = resource_types.id "
            "WHERE resources.filename = :value AND resource_types.name = :resource_type "
            "ORDER BY resources.id"))) {
        return result;
    }
    query.bindValue(PkString(":value"), PkVariant(filename));
    query.bindValue(PkString(":resource_type"), PkVariant(d->resourceType));
    if (!query.exec()) {
        return result;
    }
    while (query.next()) {
        KoResourceSP resource = resourceForId(query.value(0).toInt());
        if (resource) {
            result.append(resource);
        }
    }
    return result;
}

PkVector<KoResourceSP> KisAllResourcesModel::resourcesForName(
    const PkString &name) const
{
    PkVector<KoResourceSP> result;
    if (name.isEmpty()) {
        return result;
    }

    PkSqlQuery query;
    if (!query.prepare(PkString(
            "SELECT resources.id FROM resources "
            "JOIN resource_types ON resources.resource_type_id = resource_types.id "
            "WHERE resources.name = :value AND resource_types.name = :resource_type "
            "ORDER BY resources.id"))) {
        return result;
    }
    query.bindValue(PkString(":value"), PkVariant(name));
    query.bindValue(PkString(":resource_type"), PkVariant(d->resourceType));
    if (!query.exec()) {
        return result;
    }
    while (query.next()) {
        KoResourceSP resource = resourceForId(query.value(0).toInt());
        if (resource) {
            result.append(resource);
        }
    }
    return result;
}

PkVector<KoResourceSP> KisAllResourcesModel::resourcesForMD5(
    const PkString &md5sum) const
{
    PkVector<KoResourceSP> result;
    if (md5sum.isEmpty()) {
        return result;
    }

    PkSqlQuery query;
    if (!query.prepare(PkString(
            "SELECT DISTINCT versioned_resources.resource_id "
            "FROM versioned_resources "
            "JOIN resources ON resources.id = versioned_resources.resource_id "
            "JOIN resource_types ON resource_types.id = resources.resource_type_id "
            "WHERE versioned_resources.md5sum = :value "
            "AND resource_types.name = :resource_type "
            "ORDER BY versioned_resources.resource_id"))) {
        return result;
    }
    query.bindValue(PkString(":value"), PkVariant(md5sum));
    query.bindValue(PkString(":resource_type"), PkVariant(d->resourceType));
    if (!query.exec()) {
        return result;
    }
    while (query.next()) {
        KoResourceSP resource = resourceForId(query.value(0).toInt());
        if (resource) {
            result.append(resource);
        }
    }
    return result;
}

PkVector<KisTagSP> KisAllResourcesModel::tagsForResource(int resourceId) const
{
    PkVector<KisTagSP> result;
    PkSqlQuery query;
    if (!query.prepare(PkString(
            "SELECT tags.url FROM tags "
            "JOIN resource_tags ON resource_tags.tag_id = tags.id "
            "JOIN resource_types ON resource_types.id = tags.resource_type_id "
            "WHERE tags.active = 1 AND resource_tags.active = 1 "
            "AND resource_tags.resource_id = :resource_id "
            "AND resource_types.name = :resource_type "
            "ORDER BY tags.id"))) {
        return result;
    }
    query.bindValue(PkString(":resource_id"), PkVariant(resourceId));
    query.bindValue(PkString(":resource_type"), PkVariant(d->resourceType));
    if (!query.exec()) {
        return result;
    }

    KisResourceLocator *locator = KisResourceLocator::instance();
    while (locator && query.next()) {
        KisTagSP tag = locator->tagForUrl(query.value(0).toString(), d->resourceType);
        if (tag && tag->valid()) {
            result.append(tag);
        }
    }
    return result;
}

bool KisAllResourcesModel::setResourceActive(int resourceId, bool value)
{
    KisResourceLocator *locator = KisResourceLocator::instance();
    return locator && resourceId >= 0 &&
        locator->setResourceActive(resourceId, value);
}

KoResourceSP KisAllResourcesModel::importResourceFile(const PkString &filename,
                                                       bool allowOverwrite,
                                                       const PkString &storageId)
{
    KisResourceLocator *locator = KisResourceLocator::instance();
    KoResourceSP resource = locator
        ? locator->importResourceFromFile(d->resourceType,
                                          filename,
                                          allowOverwrite,
                                          storageId)
        : KoResourceSP();
    refresh();
    return resource;
}

KoResourceSP KisAllResourcesModel::importResource(const PkString &filename,
                                                   PkStream *device,
                                                   bool allowOverwrite,
                                                   const PkString &storageId)
{
    KisResourceLocator *locator = KisResourceLocator::instance();
    KoResourceSP resource = locator
        ? locator->importResource(d->resourceType,
                                  filename,
                                  device,
                                  allowOverwrite,
                                  storageId)
        : KoResourceSP();
    refresh();
    return resource;
}

bool KisAllResourcesModel::importWillOverwriteResource(
    const PkString &filename,
    const PkString &storageLocation) const
{
    KisResourceLocator *locator = KisResourceLocator::instance();
    return locator && locator->importWillOverwriteResource(d->resourceType,
                                                           filename,
                                                           storageLocation);
}

bool KisAllResourcesModel::exportResource(KoResourceSP resource,
                                          PkStream *device)
{
    KisResourceLocator *locator = KisResourceLocator::instance();
    return locator && locator->exportResource(resource, device);
}

bool KisAllResourcesModel::addResource(KoResourceSP resource,
                                       const PkString &storageId)
{
    if (!resource || !resource->valid()) {
        return false;
    }
    KisResourceLocator *locator = KisResourceLocator::instance();
    const bool result = locator &&
        locator->addResource(d->resourceType, resource, storageId);
    refresh();
    return result;
}

bool KisAllResourcesModel::addResourceDeduplicateFileName(
    KoResourceSP resource,
    const PkString &storageId)
{
    if (!resource || !resource->valid()) {
        return false;
    }
    KisResourceLocator *locator = KisResourceLocator::instance();
    const bool result = locator && locator->addResourceDeduplicateFileName(
        d->resourceType, resource, storageId);
    refresh();
    return result;
}

bool KisAllResourcesModel::updateResource(KoResourceSP resource)
{
    if (!resource || !resource->valid()) {
        return false;
    }
    KisResourceLocator *locator = KisResourceLocator::instance();
    const bool result = locator && locator->updateResource(d->resourceType, resource);
    if (result) {
        refresh();
    }
    return result;
}

bool KisAllResourcesModel::reloadResource(KoResourceSP resource)
{
    if (!resource || !resource->valid()) {
        return false;
    }
    KisResourceLocator *locator = KisResourceLocator::instance();
    const bool result = locator &&
        locator->reloadResource(d->resourceType, resource);
    if (result) {
        refresh();
    }
    return result;
}

bool KisAllResourcesModel::renameResource(KoResourceSP resource,
                                          const PkString &name)
{
    if (!resource || !resource->valid() || name.isEmpty()) {
        return false;
    }
    resource->setName(name);
    return updateResource(resource);
}

bool KisAllResourcesModel::setResourceMetaData(
    KoResourceSP resource,
    PkMap<PkString, PkVariant> metadata)
{
    KisResourceLocator *locator = KisResourceLocator::instance();
    const bool result = resource && resource->resourceId() >= 0 && locator &&
        locator->setMetaDataForResource(resource->resourceId(), metadata);
    if (result) {
        refresh();
    }
    return result;
}

bool KisAllResourcesModel::refresh()
{
    PkSqlQuery query;
    if (!query.prepare(PkString(
            "SELECT resources.id, resources.storage_id, resources.name, "
            "resources.filename, resources.tooltip, resources.status, "
            "resources.md5sum, storages.location, "
            "resource_types.name AS resource_type, "
            "resources.status AS resource_active, "
            "storages.active AS storage_active "
            "FROM resources "
            "JOIN resource_types ON resources.resource_type_id = resource_types.id "
            "JOIN storages ON resources.storage_id = storages.id "
            "WHERE resource_types.name = :resource_type "
            "GROUP BY resources.name, resources.filename, resources.md5sum "
            "ORDER BY resources.id"))) {
        return false;
    }
    query.bindValue(PkString(":resource_type"), PkVariant(d->resourceType));
    if (!query.exec()) {
        return false;
    }

    PkVector<KisResourceRecord> replacement;
    while (query.next()) {
        KisResourceRecord record =
            KisResourceQueryMapper::resourceFromQuery(query, false);
        for (const KisTagSP &tag : tagsForResource(record.id)) {
            if (tag) {
                record.tags.append(tag->name());
            }
        }
        replacement.append(record);
    }
    d->records = replacement;
    d->closed = false;
    return true;
}

void KisAllResourcesModel::closeQuery()
{
    d->records.clear();
    d->closed = true;
}

void KisAllResourcesModel::storageActiveStateChanged(const PkString &location)
{
    (void)location;
    refresh();
}

void KisAllResourcesModel::storageResynchronized(const PkString &storage,
                                                 bool bulk)
{
    (void)storage;
    if (!bulk) {
        refresh();
    }
}

void KisAllResourcesModel::storagesBulkSynchronizationFinished()
{
    refresh();
}

void KisAllResourcesModel::beginExternalResourceImport(
    const PkString &resourceType,
    int count)
{
    (void)resourceType;
    (void)count;
}

void KisAllResourcesModel::endExternalResourceImport(
    const PkString &resourceType)
{
    if (resourceType == d->resourceType) {
        refresh();
    }
}

void KisAllResourcesModel::beginExternalResourceRemove(
    const PkString &resourceType,
    PkVector<int> resourceIds)
{
    (void)resourceType;
    (void)resourceIds;
}

void KisAllResourcesModel::endExternalResourceRemove(
    const PkString &resourceType)
{
    if (resourceType == d->resourceType) {
        refresh();
    }
}

void KisAllResourcesModel::slotResourceActiveStateChanged(
    const PkString &resourceType,
    int resourceId)
{
    (void)resourceId;
    if (resourceType == d->resourceType) {
        refresh();
    }
}

struct KisResourceModel::Private
{
    KisAllResourcesModel *source = nullptr;
    ResourceFilter resourceFilter = ShowActiveResources;
    StorageFilter storageFilter = ShowActiveStorages;
    bool showOnlyUntaggedResources = false;
};

KisResourceModel::KisResourceModel(const PkString &type)
    : d(new Private)
{
    d->source = KisResourceModelProvider::resourceModel(type);
}

KisResourceModel::~KisResourceModel()
{
    delete d;
}

void KisResourceModel::setResourceFilter(ResourceFilter filter)
{
    d->resourceFilter = filter;
}

void KisResourceModel::setStorageFilter(StorageFilter filter)
{
    d->storageFilter = filter;
}

void KisResourceModel::showOnlyUntaggedResources(bool showOnlyUntagged)
{
    d->showOnlyUntaggedResources = showOnlyUntagged;
}

bool KisResourceModel::accepts(const KisResourceRecord &record) const
{
    if (d->showOnlyUntaggedResources && !record.tags.isEmpty()) {
        return false;
    }
    if (d->resourceFilter != ShowAllResources) {
        const bool wanted = d->resourceFilter == ShowActiveResources;
        if (record.resourceActive != wanted) {
            return false;
        }
    }
    if (d->storageFilter != ShowAllStorages) {
        const bool wanted = d->storageFilter == ShowActiveStorages;
        if (record.storageActive != wanted) {
            return false;
        }
    }
    return true;
}

PkVector<KisResourceRecord> KisResourceModel::records() const
{
    PkVector<KisResourceRecord> result;
    if (!d->source) {
        return result;
    }
    for (const KisResourceRecord &record : d->source->records()) {
        if (accepts(record)) {
            result.append(record);
        }
    }
    return result;
}

PkVector<KoResourceSP> KisResourceModel::resources() const
{
    PkVector<KoResourceSP> result;
    for (const KisResourceRecord &record : records()) {
        KoResourceSP resource = d->source->resourceForId(record.id);
        if (resource) {
            result.append(resource);
        }
    }
    return result;
}

KoResourceSP KisResourceModel::resourceForId(int id) const
{
    for (const KisResourceRecord &record : records()) {
        if (record.id == id) {
            return d->source->resourceForId(id);
        }
    }
    return KoResourceSP();
}

bool KisResourceModel::setResourceActive(int resourceId, bool value)
{
    return d->source && d->source->setResourceActive(resourceId, value);
}

KoResourceSP KisResourceModel::importResourceFile(const PkString &filename,
                                                   bool allowOverwrite,
                                                   const PkString &storageId)
{
    return d->source
        ? d->source->importResourceFile(filename, allowOverwrite, storageId)
        : KoResourceSP();
}

KoResourceSP KisResourceModel::importResource(const PkString &filename,
                                               PkStream *device,
                                               bool allowOverwrite,
                                               const PkString &storageId)
{
    return d->source
        ? d->source->importResource(filename, device, allowOverwrite, storageId)
        : KoResourceSP();
}

bool KisResourceModel::importWillOverwriteResource(
    const PkString &filename,
    const PkString &storageLocation) const
{
    return d->source &&
        d->source->importWillOverwriteResource(filename, storageLocation);
}

bool KisResourceModel::exportResource(KoResourceSP resource, PkStream *device)
{
    return d->source && d->source->exportResource(resource, device);
}

bool KisResourceModel::addResource(KoResourceSP resource,
                                   const PkString &storageId)
{
    if (!d->source || !resource || !resource->valid()) {
        return false;
    }

    PkSqlQuery query;
    if (query.prepare(PkString(
            "SELECT resources.id, resources.md5sum, storages.location "
            "FROM resources "
            "JOIN storages ON storages.id = resources.storage_id "
            "JOIN resource_types ON resource_types.id = resources.resource_type_id "
            "WHERE resources.name = :name AND resources.status = 0 "
            "AND resource_types.name = :resource_type ORDER BY resources.id"))) {
        query.bindValue(PkString(":name"), PkVariant(resource->name()));
        query.bindValue(PkString(":resource_type"),
                        PkVariant(resource->resourceType().first));
        if (query.exec() && query.first()) {
            const int id = query.value(0).toInt();
            PkSqlQuery versionQuery;
            if (versionQuery.prepare(PkString(
                    "SELECT MAX(version) FROM versioned_resources "
                    "WHERE resource_id = :id"))) {
                versionQuery.bindValue(PkString(":id"), PkVariant(id));
                if (versionQuery.exec() && versionQuery.first()) {
                    resource->setResourceId(id);
                    resource->setVersion(versionQuery.value(0).toInt());
                    resource->setMD5Sum(query.value(1).toString());
                    resource->setStorageLocation(query.value(2).toString());
                    resource->setActive(true);
                    return d->source->updateResource(resource);
                }
            }
        }
    }

    return d->source->addResource(resource, storageId);
}

bool KisResourceModel::addResourceDeduplicateFileName(
    KoResourceSP resource,
    const PkString &storageId)
{
    return d->source &&
        d->source->addResourceDeduplicateFileName(resource, storageId);
}

bool KisResourceModel::updateResource(KoResourceSP resource)
{
    return d->source && d->source->updateResource(resource);
}

bool KisResourceModel::reloadResource(KoResourceSP resource)
{
    return d->source && d->source->reloadResource(resource);
}

bool KisResourceModel::renameResource(KoResourceSP resource,
                                      const PkString &name)
{
    return d->source && d->source->renameResource(resource, name);
}

bool KisResourceModel::setResourceMetaData(
    KoResourceSP resource,
    PkMap<PkString, PkVariant> metadata)
{
    return d->source && d->source->setResourceMetaData(resource, metadata);
}

PkVector<KoResourceSP> KisResourceModel::resourcesMatching(
    const PkString &value,
    Columns column) const
{
    PkVector<KoResourceSP> result;
    for (const KisResourceRecord &record : records()) {
        PkString candidate;
        switch (column) {
        case Filename:
            candidate = record.filename;
            break;
        case Name:
            candidate = record.name;
            break;
        case MD5:
            candidate = record.md5;
            break;
        default:
            break;
        }
        if (candidate == value) {
            KoResourceSP resource = d->source->resourceForId(record.id);
            if (resource) {
                result.append(resource);
            }
        }
    }
    return result;
}

PkVector<KoResourceSP> KisResourceModel::resourcesForFilename(
    const PkString &filename) const
{
    return resourcesMatching(filename, Filename);
}

PkVector<KoResourceSP> KisResourceModel::resourcesForName(
    const PkString &name) const
{
    return resourcesMatching(name, Name);
}

PkVector<KoResourceSP> KisResourceModel::resourcesForMD5(
    const PkString &md5sum) const
{
    PkVector<KoResourceSP> result;
    if (!d->source) {
        return result;
    }

    PkVector<int> acceptedIds;
    for (const KisResourceRecord &record : records()) {
        acceptedIds.append(record.id);
    }
    for (const KoResourceSP &resource : d->source->resourcesForMD5(md5sum)) {
        if (resource && acceptedIds.contains(resource->resourceId())) {
            result.append(resource);
        }
    }
    return result;
}

PkVector<KisTagSP> KisResourceModel::tagsForResource(int resourceId) const
{
    return d->source ? d->source->tagsForResource(resourceId)
                     : PkVector<KisTagSP>();
}
