/*
 * SPDX-FileCopyrightText: 2018 boud <boud@valdyas.org>
 * SPDX-FileCopyrightText: 2020 Agata Cacko <cacko.azh@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisTagModel.h"

#include <PkSqlQuery.h>

#include <algorithm>

#include "KisResourceCacheDb.h"
#include "KisResourceLocator.h"
#include "KisResourceModelProvider.h"
#include "KisStorageModel.h"
#include "KisTagResourceModel.h"

struct KisAllTagsModel::Private
{
    PkString resourceType;
    PkVector<KisTagSP> tags;
};

KisAllTagsModel::KisAllTagsModel(const PkString &resourceType,
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
                          &KisAllTagsModel::storageChanged);
        PkObject::connect(locator,
                          &KisResourceLocator::storageRemoved,
                          this,
                          &KisAllTagsModel::storageChanged);
    }
    if (storageModel) {
        PkObject::connect(storageModel,
                          &KisStorageModel::storageEnabled,
                          this,
                          &KisAllTagsModel::storageChanged);
        PkObject::connect(storageModel,
                          &KisStorageModel::storageDisabled,
                          this,
                          &KisAllTagsModel::storageChanged);
    }
    refresh();
}

KisAllTagsModel::~KisAllTagsModel()
{
    delete d;
}

KisTagSP KisAllTagsModel::specialTag(Ids id) const
{
    KisTagSP tag(new KisTag);
    if (id == All) {
        tag->setName(PkString("All"));
        tag->setUrl(urlAll());
        tag->setComment(PkString("All resources"));
    } else {
        tag->setName(PkString("All untagged"));
        tag->setUrl(urlAllUntagged());
        tag->setComment(PkString("All untagged resources"));
    }
    tag->setResourceType(d->resourceType);
    tag->setId(id);
    tag->setActive(true);
    tag->setValid(true);
    return tag;
}

PkVector<KisTagSP> KisAllTagsModel::tags() const
{
    PkVector<KisTagSP> result;
    result.append(specialTag(All));
    result.append(specialTag(AllUntagged));
    result.append(d->tags);
    return result;
}

KisTagSP KisAllTagsModel::tagForUrl(const PkString &tagUrl) const
{
    if (tagUrl.isEmpty()) {
        return KisTagSP();
    }
    if (tagUrl == urlAll()) {
        return specialTag(All);
    }
    if (tagUrl == urlAllUntagged()) {
        return specialTag(AllUntagged);
    }
    KisResourceLocator *locator = KisResourceLocator::instance();
    return locator ? locator->tagForUrl(tagUrl, d->resourceType) : KisTagSP();
}

KisTagSP KisAllTagsModel::addTag(const PkString &tagName,
                                 bool allowOverwrite,
                                 PkVector<KoResourceSP> taggedResources)
{
    KisTagSP tag(new KisTag);
    tag->setName(tagName);
    tag->setUrl(tagName);
    tag->setValid(true);
    tag->setActive(true);
    tag->setResourceType(d->resourceType);
    return addTag(tag, allowOverwrite, taggedResources) ? tag : KisTagSP();
}

bool KisAllTagsModel::addTag(const KisTagSP &tag,
                             bool allowOverwrite,
                             PkVector<KoResourceSP> taggedResources)
{
    if (!tag || !tag->valid()) {
        return false;
    }

    if (!KisResourceCacheDb::hasTag(tag->url(), d->resourceType)) {
        if (!KisResourceCacheDb::addTag(d->resourceType, PkString(), tag)) {
            return false;
        }
        refresh();
    } else if (allowOverwrite) {
        KisTagSP existing = tagForUrl(tag->url());
        if (!existing) {
            return false;
        }
        if (!setTagActive(existing)) {
            return false;
        }
        untagAllResources(existing);
        tag->setComment(existing->comment());
        tag->setFilename(existing->filename());
    } else {
        return false;
    }

    KisTagSP stored = tagForUrl(tag->url());
    if (!stored) {
        return false;
    }
    tag->setId(stored->id());
    tag->setActive(stored->active());
    tag->setValid(stored->valid());

    PkVector<int> resourceIds;
    for (const KoResourceSP &resource : taggedResources) {
        if (resource && resource->valid() && resource->resourceId() >= 0) {
            resourceIds.append(resource->resourceId());
        }
    }
    if (!resourceIds.isEmpty()) {
        KisTagResourceModel relationModel(d->resourceType);
        if (!relationModel.tagResources(tag, resourceIds)) {
            return false;
        }
    }
    return true;
}

bool KisAllTagsModel::changeTagActive(const KisTagSP &tag, bool active)
{
    if (!tag || !tag->valid() || tag->id() < 0) {
        return false;
    }
    PkSqlQuery query;
    if (!query.prepare(PkString(
            "UPDATE tags SET active = :active WHERE id = :id"))) {
        return false;
    }
    query.bindValue(PkString(":active"), PkVariant(active));
    query.bindValue(PkString(":id"), PkVariant(tag->id()));
    if (!query.exec()) {
        return false;
    }

    KisResourceLocator *locator = KisResourceLocator::instance();
    if (locator) {
        locator->purgeTag(tag->url(), d->resourceType);
    }
    tag->setActive(active);
    const bool tagsRefreshed = refresh();
    const bool relationsRefreshed =
        KisResourceModelProvider::refreshTagResourceModel(d->resourceType);
    const bool resourcesRefreshed =
        KisResourceModelProvider::refreshResourceModel(d->resourceType);
    return tagsRefreshed && relationsRefreshed && resourcesRefreshed;
}

bool KisAllTagsModel::setTagActive(const KisTagSP &tag)
{
    return changeTagActive(tag, true);
}

bool KisAllTagsModel::setTagInactive(const KisTagSP &tag)
{
    return changeTagActive(tag, false);
}

void KisAllTagsModel::untagAllResources(const KisTagSP &tag)
{
    if (!tag) {
        return;
    }
    KisTagResourceModel relationModel(d->resourceType);
    relationModel.setTagsFilter(PkVector<int>{tag->id()});
    PkVector<int> resourceIds;
    for (const KisTagResourceRecord &record : relationModel.relations()) {
        if (!resourceIds.contains(record.resourceId)) {
            resourceIds.append(record.resourceId);
        }
    }
    if (!resourceIds.isEmpty()) {
        relationModel.untagResources(tag, resourceIds);
    }
}

bool KisAllTagsModel::renameTag(const KisTagSP &tag,
                                const PkString &newName,
                                bool allowOverwrite)
{
    if (!tag || !tag->valid() || tag->id() < 0 || newName.isEmpty()) {
        return false;
    }

    KisTagSP destination = tagForUrl(newName);
    if (destination && destination->active() && !allowOverwrite) {
        return false;
    }
    if (!destination) {
        destination = addTag(newName, false, {});
    } else {
        if (!destination->active() && !setTagActive(destination)) {
            return false;
        }
        untagAllResources(destination);
    }
    if (!destination) {
        return false;
    }

    KisTagResourceModel relationModel(d->resourceType);
    relationModel.setTagsFilter(PkVector<int>{tag->id()});
    PkVector<int> resourceIds;
    for (const KisTagResourceRecord &record : relationModel.relations()) {
        if (!resourceIds.contains(record.resourceId)) {
            resourceIds.append(record.resourceId);
        }
    }
    if (!resourceIds.isEmpty() &&
        (!relationModel.tagResources(destination, resourceIds) ||
         !relationModel.untagResources(tag, resourceIds))) {
        return false;
    }
    return setTagInactive(tag);
}

void KisAllTagsModel::storageChanged(const PkString &location)
{
    (void)location;
    refresh();
}

bool KisAllTagsModel::refresh()
{
    PkSqlQuery query;
    if (!query.prepare(PkString(
            "SELECT tags.url FROM tags "
            "JOIN resource_types ON resource_types.id = tags.resource_type_id "
            "WHERE resource_types.name = :resource_type ORDER BY tags.id"))) {
        return false;
    }
    query.bindValue(PkString(":resource_type"), PkVariant(d->resourceType));
    if (!query.exec()) {
        return false;
    }

    PkVector<KisTagSP> replacement;
    KisResourceLocator *locator = KisResourceLocator::instance();
    while (locator && query.next()) {
        KisTagSP tag = locator->tagForUrl(query.value(0).toString(), d->resourceType);
        if (tag && tag->valid()) {
            replacement.append(tag);
        }
    }
    d->tags = replacement;
    return true;
}

void KisAllTagsModel::closeQuery()
{
    d->tags.clear();
}

struct KisTagModel::Private
{
    KisAllTagsModel *source = nullptr;
    TagFilter tagFilter = ShowActiveTags;
    StorageFilter storageFilter = ShowActiveStorages;
};

KisTagModel::KisTagModel(const PkString &type)
    : d(new Private)
{
    d->source = KisResourceModelProvider::tagModel(type);
}

KisTagModel::~KisTagModel()
{
    delete d;
}

void KisTagModel::setTagFilter(TagFilter filter)
{
    d->tagFilter = filter;
}

void KisTagModel::setStorageFilter(StorageFilter filter)
{
    d->storageFilter = filter;
}

bool KisTagModel::accepts(const KisTagSP &tag) const
{
    if (!tag) {
        return false;
    }
    if (tag->id() < 0) {
        return true;
    }

    if (d->tagFilter != ShowAllTags) {
        const bool wanted = d->tagFilter == ShowActiveTags;
        if (tag->active() != wanted) {
            return false;
        }
    }
    if (d->storageFilter == ShowAllStorages) {
        return true;
    }

    PkSqlQuery query;
    if (!query.prepare(PkString(
            "SELECT COUNT(*) FROM tags_storages "
            "JOIN storages ON storages.id = tags_storages.storage_id "
            "WHERE tags_storages.tag_id = :tag_id AND storages.active = 1"))) {
        return true;
    }
    query.bindValue(PkString(":tag_id"), PkVariant(tag->id()));
    const bool hasActiveStorage = query.exec() && query.first() &&
        query.value(0).toInt() > 0;
    const bool wanted = d->storageFilter == ShowActiveStorages;
    return hasActiveStorage == wanted;
}

PkVector<KisTagSP> KisTagModel::tags() const
{
    PkVector<KisTagSP> special;
    PkVector<KisTagSP> ordinary;
    if (!d->source) {
        return ordinary;
    }
    for (const KisTagSP &tag : d->source->tags()) {
        if (!accepts(tag)) {
            continue;
        }
        if (tag->id() < 0) {
            special.append(tag);
        } else {
            ordinary.append(tag);
        }
    }
    std::sort(ordinary.begin(), ordinary.end(),
              [](const KisTagSP &left, const KisTagSP &right) {
                  return left->name().toLower() < right->name().toLower();
              });
    special.append(ordinary);
    return special;
}

KisTagSP KisTagModel::tagForUrl(const PkString &url) const
{
    return d->source ? d->source->tagForUrl(url) : KisTagSP();
}

KisTagSP KisTagModel::addTag(const PkString &tagName,
                             bool allowOverwrite,
                             PkVector<KoResourceSP> taggedResources)
{
    return d->source
        ? d->source->addTag(tagName, allowOverwrite, taggedResources)
        : KisTagSP();
}

bool KisTagModel::addTag(const KisTagSP &tag,
                         bool allowOverwrite,
                         PkVector<KoResourceSP> taggedResources)
{
    return d->source &&
        d->source->addTag(tag, allowOverwrite, taggedResources);
}

bool KisTagModel::setTagInactive(const KisTagSP &tag)
{
    return d->source && d->source->setTagInactive(tag);
}

bool KisTagModel::setTagActive(const KisTagSP &tag)
{
    return d->source && d->source->setTagActive(tag);
}

bool KisTagModel::renameTag(const KisTagSP &tag,
                            const PkString &newName,
                            bool allowOverwrite)
{
    return d->source && d->source->renameTag(tag, newName, allowOverwrite);
}

bool KisTagModel::changeTagActive(const KisTagSP &tag, bool active)
{
    return d->source && d->source->changeTagActive(tag, active);
}
