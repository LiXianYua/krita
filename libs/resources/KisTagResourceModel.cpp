/*
 * SPDX-FileCopyrightText: 2020 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-FileCopyrightText: 2023 L. E. Segovia <amy@amyspark.me>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KisTagResourceModel.h"

#include <PkSqlDatabase.h>
#include <PkSqlQuery.h>

#include "KisDatabaseTransactionLock.h"
#include "KisResourceLocator.h"
#include "KisResourceModelProvider.h"
#include "KisResourceQueryMapper.h"
#include "KisStorageModel.h"

struct KisAllTagResourceModel::Private
{
    PkString resourceType;
    PkVector<KisTagResourceRecord> relations;
};

KisAllTagResourceModel::KisAllTagResourceModel(const PkString &resourceType,
                                               PkObject *parent)
    : PkObject(parent)
    , d(new Private)
{
    d->resourceType = resourceType;

    KisResourceLocator *locator = KisResourceLocator::instance();
    KisStorageModel *storageModel = KisStorageModel::instance();
    if (locator) {
        PkObject::connect(locator,
                          &KisResourceLocator::storageAdded,
                          this,
                          &KisAllTagResourceModel::storageChanged);
        PkObject::connect(locator,
                          &KisResourceLocator::storageRemoved,
                          this,
                          &KisAllTagResourceModel::storageChanged);
        PkObject::connect(locator,
                          &KisResourceLocator::resourceActiveStateChanged,
                          this,
                          &KisAllTagResourceModel::slotResourceActiveStateChanged);
    }
    if (storageModel) {
        PkObject::connect(storageModel,
                          &KisStorageModel::storageEnabled,
                          this,
                          &KisAllTagResourceModel::storageChanged);
        PkObject::connect(storageModel,
                          &KisStorageModel::storageDisabled,
                          this,
                          &KisAllTagResourceModel::storageChanged);
    }
    refresh();
}

KisAllTagResourceModel::~KisAllTagResourceModel()
{
    delete d;
}

PkVector<KisTagResourceRecord> KisAllTagResourceModel::relations() const
{
    return d->relations;
}

int KisAllTagResourceModel::isResourceTagged(const KisTagSP &tag,
                                             int resourceId)
{
    if (!tag || tag->id() < 0 || resourceId < 0) {
        return -1;
    }
    PkSqlQuery query;
    if (!query.prepare(PkString(
            "SELECT active FROM resource_tags "
            "WHERE resource_id = :resource_id AND tag_id = :tag_id"))) {
        return -1;
    }
    query.bindValue(PkString(":resource_id"), PkVariant(resourceId));
    query.bindValue(PkString(":tag_id"), PkVariant(tag->id()));
    if (!query.exec() || !query.first()) {
        return -1;
    }
    return query.value(0).toBool() ? 1 : 0;
}

bool KisAllTagResourceModel::tagResources(const KisTagSP &tag,
                                          const PkVector<int> &resourceIds)
{
    if (!tag || !tag->valid() || tag->id() < 0) {
        return false;
    }

    KisDatabaseTransactionLock transaction(
        PkSqlDatabase::database(PkSqlDatabase::defaultConnection, false));
    if (!transaction.transactionStarted()) {
        return false;
    }

    PkSqlQuery insert;
    PkSqlQuery reactivate;
    if (!insert.prepare(PkString(
            "INSERT INTO resource_tags(resource_id, tag_id, active) "
            "VALUES(:resource_id, :tag_id, 1)")) ||
        !reactivate.prepare(PkString(
            "UPDATE resource_tags SET active = 1 "
            "WHERE resource_id = :resource_id AND tag_id = :tag_id"))) {
        return false;
    }

    for (int resourceId : resourceIds) {
        if (resourceId < 0) {
            return false;
        }
        const int state = isResourceTagged(tag, resourceId);
        if (state == 1) {
            continue;
        }
        PkSqlQuery &query = state == 0 ? reactivate : insert;
        query.bindValue(PkString(":resource_id"), PkVariant(resourceId));
        query.bindValue(PkString(":tag_id"), PkVariant(tag->id()));
        if (!query.exec()) {
            return false;
        }
    }

    if (!transaction.commit()) {
        return false;
    }
    const bool relationsRefreshed = refresh();
    const bool resourcesRefreshed =
        KisResourceModelProvider::refreshResourceModel(d->resourceType);
    return relationsRefreshed && resourcesRefreshed;
}

bool KisAllTagResourceModel::untagResources(const KisTagSP &tag,
                                            const PkVector<int> &resourceIds)
{
    if (!tag || !tag->valid() || tag->id() < 0) {
        return false;
    }

    KisDatabaseTransactionLock transaction(
        PkSqlDatabase::database(PkSqlDatabase::defaultConnection, false));
    if (!transaction.transactionStarted()) {
        return false;
    }
    PkSqlQuery query;
    if (!query.prepare(PkString(
            "UPDATE resource_tags SET active = 0 "
            "WHERE tag_id = :tag_id AND resource_id = :resource_id"))) {
        return false;
    }
    for (int resourceId : resourceIds) {
        if (resourceId < 0 || isResourceTagged(tag, resourceId) != 1) {
            continue;
        }
        query.bindValue(PkString(":tag_id"), PkVariant(tag->id()));
        query.bindValue(PkString(":resource_id"), PkVariant(resourceId));
        if (!query.exec()) {
            return false;
        }
    }
    if (!transaction.commit()) {
        return false;
    }
    const bool relationsRefreshed = refresh();
    const bool resourcesRefreshed =
        KisResourceModelProvider::refreshResourceModel(d->resourceType);
    return relationsRefreshed && resourcesRefreshed;
}

void KisAllTagResourceModel::storageChanged(const PkString &location)
{
    (void)location;
    refresh();
}

void KisAllTagResourceModel::slotResourceActiveStateChanged(
    const PkString &resourceType,
    int resourceId)
{
    (void)resourceId;
    if (resourceType == d->resourceType) {
        refresh();
    }
}

bool KisAllTagResourceModel::refresh()
{
    PkSqlQuery query;
    if (!query.prepare(PkString(
            "WITH selected_resources AS ("
            "SELECT resource_tags.tag_id AS tag_id, "
            "MIN(resources.id) AS resource_id, "
            "MIN(resource_tags.id) AS first_relation_id "
            "FROM resource_tags "
            "JOIN resources ON resources.id = resource_tags.resource_id "
            "JOIN storages ON storages.id = resources.storage_id "
            "JOIN resource_types ON resource_types.id = resources.resource_type_id "
            "JOIN tags ON tags.id = resource_tags.tag_id "
            "AND tags.resource_type_id = resource_types.id "
            "WHERE resource_types.name = :resource_type "
            "AND resource_tags.active = 1 "
            "GROUP BY resource_tags.tag_id, resources.name, resources.filename, "
            "resources.md5sum) "
            "SELECT selected_resources.tag_id AS tag_id, "
            "resources.id AS resource_id, "
            "resources.storage_id AS storage_id, "
            "resources.name AS resource_name, "
            "resources.filename AS resource_filename, "
            "resources.tooltip AS resource_tooltip, "
            "resources.status AS resource_active, "
            "resources.md5sum AS resource_md5sum, "
            "storages.location AS location, "
            "storages.active AS resource_storage_active, "
            "resource_types.name AS resource_type, "
            "tags.url AS tag_url, tags.active AS tag_active, "
            "tags.name AS tag_name, "
            "tag_translations.name AS translated_name "
            "FROM selected_resources "
            "JOIN resources ON resources.id = selected_resources.resource_id "
            "JOIN resource_types ON resource_types.id = resources.resource_type_id "
            "JOIN tags ON tags.id = selected_resources.tag_id "
            "AND tags.resource_type_id = resource_types.id "
            "JOIN storages ON storages.id = resources.storage_id "
            "LEFT JOIN tag_translations ON tag_translations.tag_id = tags.id "
            "AND tag_translations.language = :language "
            "ORDER BY selected_resources.first_relation_id"))) {
        return false;
    }
    query.bindValue(PkString(":resource_type"), PkVariant(d->resourceType));
    query.bindValue(PkString(":language"), PkVariant(KisTag::currentLocale()));
    if (!query.exec()) {
        return false;
    }

    PkVector<KisTagResourceRecord> replacement;
    KisResourceLocator *locator = KisResourceLocator::instance();
    KisAllResourcesModel *resourceModel =
        KisResourceModelProvider::resourceModel(d->resourceType);
    while (query.next()) {
        KisTagResourceRecord record;
        record.tagId = query.value(PkString("tag_id")).toInt();
        record.resourceId = query.value(PkString("resource_id")).toInt();
        record.resource = KisResourceQueryMapper::resourceFromQuery(query, true);
        record.tagActive = query.value(PkString("tag_active")).toBool();
        record.resourceActive = query.value(PkString("resource_active")).toBool();
        record.resourceStorageActive =
            query.value(PkString("resource_storage_active")).toBool();
        record.tagName = query.value(PkString("translated_name")).toString();
        if (record.tagName.isEmpty()) {
            record.tagName = query.value(PkString("tag_name")).toString();
        }
        if (locator) {
            record.tag = locator->tagForUrl(
                query.value(PkString("tag_url")).toString(), d->resourceType);
        }
        if (resourceModel) {
            for (const KisTagSP &tag : resourceModel->tagsForResource(record.resourceId)) {
                if (tag) {
                    record.resource.tags.append(tag->name());
                }
            }
        }
        replacement.append(record);
    }
    d->relations = replacement;
    return true;
}

void KisAllTagResourceModel::closeQuery()
{
    d->relations.clear();
}

struct KisTagResourceModel::Private
{
    PkString resourceType;
    KisAllTagResourceModel *source = nullptr;
    KisResourceModel *resourceModel = nullptr;
    PkVector<int> tagIds;
    PkVector<int> resourceIds;
    TagFilter tagFilter = ShowActiveTags;
    ResourceFilter resourceFilter = ShowActiveResources;
    StorageFilter storageFilter = ShowActiveStorages;
};

KisTagResourceModel::KisTagResourceModel(const PkString &resourceType)
    : d(new Private)
{
    d->resourceType = resourceType;
    d->source = KisResourceModelProvider::tagResourceModel(resourceType);
    d->resourceModel = new KisResourceModel(resourceType);
}

KisTagResourceModel::~KisTagResourceModel()
{
    delete d->resourceModel;
    delete d;
}

void KisTagResourceModel::setTagFilter(TagFilter filter)
{
    d->tagFilter = filter;
}

void KisTagResourceModel::setResourceFilter(ResourceFilter filter)
{
    d->resourceFilter = filter;
}

void KisTagResourceModel::setStorageFilter(StorageFilter filter)
{
    d->storageFilter = filter;
}

void KisTagResourceModel::setTagsFilter(const PkVector<int> &tagIds)
{
    d->tagIds = tagIds;
}

void KisTagResourceModel::setResourcesFilter(const PkVector<int> &resourceIds)
{
    d->resourceIds = resourceIds;
}

void KisTagResourceModel::setTagsFilter(const PkVector<KisTagSP> &tags)
{
    d->tagIds.clear();
    for (const KisTagSP &tag : tags) {
        if (tag && tag->valid() && tag->id() >= 0) {
            d->tagIds.append(tag->id());
        }
    }
}

void KisTagResourceModel::setResourcesFilter(
    const PkVector<KoResourceSP> &resources)
{
    d->resourceIds.clear();
    for (const KoResourceSP &resource : resources) {
        if (resource && resource->valid() && resource->resourceId() >= 0) {
            d->resourceIds.append(resource->resourceId());
        }
    }
}

bool KisTagResourceModel::accepts(const KisTagResourceRecord &record) const
{
    if (!d->tagIds.isEmpty() && !d->tagIds.contains(record.tagId)) {
        return false;
    }
    if (!d->resourceIds.isEmpty() && !d->resourceIds.contains(record.resourceId)) {
        return false;
    }
    if (d->tagFilter != ShowAllTags) {
        const bool wanted = d->tagFilter == ShowActiveTags;
        if (record.tagActive != wanted) {
            return false;
        }
    }
    if (d->resourceFilter != ShowAllResources) {
        const bool wanted = d->resourceFilter == ShowActiveResources;
        if (record.resourceActive != wanted) {
            return false;
        }
    }
    if (d->storageFilter != ShowAllStorages) {
        const bool wanted = d->storageFilter == ShowActiveStorages;
        if (record.resourceStorageActive != wanted) {
            return false;
        }
    }
    return true;
}

PkVector<KisTagResourceRecord> KisTagResourceModel::relations() const
{
    PkVector<KisTagResourceRecord> result;
    if (!d->source) {
        return result;
    }
    for (const KisTagResourceRecord &record : d->source->relations()) {
        if (accepts(record)) {
            result.append(record);
        }
    }
    return result;
}

PkVector<KisResourceRecord> KisTagResourceModel::records() const
{
    PkVector<KisResourceRecord> result;
    PkVector<int> seen;
    for (const KisTagResourceRecord &relation : relations()) {
        if (!seen.contains(relation.resourceId)) {
            seen.append(relation.resourceId);
            result.append(relation.resource);
        }
    }
    return result;
}

PkVector<KoResourceSP> KisTagResourceModel::resources() const
{
    PkVector<KoResourceSP> result;
    for (const KisResourceRecord &record : records()) {
        KoResourceSP resource = resourceForId(record.id);
        if (resource) {
            result.append(resource);
        }
    }
    return result;
}

bool KisTagResourceModel::tagResources(const KisTagSP &tag,
                                       const PkVector<int> &resourceIds)
{
    return d->source && d->source->tagResources(tag, resourceIds);
}

bool KisTagResourceModel::untagResources(const KisTagSP &tag,
                                         const PkVector<int> &resourceIds)
{
    return d->source && d->source->untagResources(tag, resourceIds);
}

int KisTagResourceModel::isResourceTagged(const KisTagSP &tag, int resourceId)
{
    return d->source ? d->source->isResourceTagged(tag, resourceId) : -1;
}

KoResourceSP KisTagResourceModel::resourceForId(int resourceId) const
{
    for (const KisResourceRecord &record : records()) {
        if (record.id == resourceId) {
            KisResourceLocator *locator = KisResourceLocator::instance();
            return locator ? locator->resourceForId(resourceId) : KoResourceSP();
        }
    }
    return KoResourceSP();
}

bool KisTagResourceModel::setResourceActive(int resourceId, bool value)
{
    return d->resourceModel && d->resourceModel->setResourceActive(resourceId, value);
}

KoResourceSP KisTagResourceModel::importResourceFile(const PkString &filename,
                                                      bool allowOverwrite,
                                                      const PkString &storageId)
{
    return d->resourceModel->importResourceFile(filename, allowOverwrite, storageId);
}

KoResourceSP KisTagResourceModel::importResource(const PkString &filename,
                                                  PkStream *device,
                                                  bool allowOverwrite,
                                                  const PkString &storageId)
{
    return d->resourceModel->importResource(filename, device, allowOverwrite, storageId);
}

bool KisTagResourceModel::importWillOverwriteResource(
    const PkString &filename,
    const PkString &storageLocation) const
{
    return d->resourceModel->importWillOverwriteResource(filename, storageLocation);
}

bool KisTagResourceModel::exportResource(KoResourceSP resource, PkStream *device)
{
    return d->resourceModel->exportResource(resource, device);
}

bool KisTagResourceModel::addResource(KoResourceSP resource,
                                      const PkString &storageId)
{
    return d->resourceModel->addResource(resource, storageId);
}

bool KisTagResourceModel::addResourceDeduplicateFileName(
    KoResourceSP resource,
    const PkString &storageId)
{
    return d->resourceModel->addResourceDeduplicateFileName(resource, storageId);
}

bool KisTagResourceModel::updateResource(KoResourceSP resource)
{
    return d->resourceModel->updateResource(resource);
}

bool KisTagResourceModel::reloadResource(KoResourceSP resource)
{
    return d->resourceModel->reloadResource(resource);
}

bool KisTagResourceModel::renameResource(KoResourceSP resource,
                                         const PkString &name)
{
    return d->resourceModel->renameResource(resource, name);
}

bool KisTagResourceModel::setResourceMetaData(
    KoResourceSP resource,
    PkMap<PkString, PkVariant> metadata)
{
    return d->resourceModel->setResourceMetaData(resource, metadata);
}
