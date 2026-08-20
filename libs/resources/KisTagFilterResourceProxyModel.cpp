/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KisTagFilterResourceProxyModel.h"

#include "KisResourceMetaDataModel.h"
#include "KisResourceModelProvider.h"
#include "KisResourceSearchBoxFilter.h"
#include "KisResourceTypes.h"
#include "KisTagModel.h"

struct KisTagFilterResourceProxyModel::Private
{
    PkString resourceType;
    KisResourceModel *ownedResourceModel = nullptr;
    KisResourceModel *resourceModel = nullptr;
    KisTagResourceModel *tagResourceModel = nullptr;
    KisResourceSearchBoxFilter *filter = nullptr;
    bool filteringWithinCurrentTag = false;
    PkMap<PkString, PkVariant> metaDataMapFilter;
    KisTagSP currentTagFilter;
    KoResourceSP currentResourceFilter;
    int storageId = -1;
    bool useStorageIdFilter = false;
};

KisTagFilterResourceProxyModel::KisTagFilterResourceProxyModel(
    const PkString &resourceType)
    : d(new Private)
{
    d->resourceType = resourceType;
    d->ownedResourceModel = new KisResourceModel(resourceType);
    d->resourceModel = d->ownedResourceModel;
    d->tagResourceModel = new KisTagResourceModel(resourceType);
    d->filter = new KisResourceSearchBoxFilter;
}

KisTagFilterResourceProxyModel::~KisTagFilterResourceProxyModel()
{
    delete d->filter;
    delete d->tagResourceModel;
    delete d->ownedResourceModel;
    delete d;
}

void KisTagFilterResourceProxyModel::setResourceFilter(ResourceFilter filter)
{
    d->resourceModel->setResourceFilter(filter);
    d->tagResourceModel->setResourceFilter(filter);
}

void KisTagFilterResourceProxyModel::setStorageFilter(StorageFilter filter)
{
    d->resourceModel->setStorageFilter(filter);
    d->tagResourceModel->setStorageFilter(filter);
}

void KisTagFilterResourceProxyModel::setResourceModel(
    KisResourceModel *resourceModel)
{
    d->resourceModel = resourceModel ? resourceModel : d->ownedResourceModel;
}

PkVector<KisResourceRecord> KisTagFilterResourceProxyModel::sourceRecords() const
{
    const bool ignoreTagFiltering =
        !d->filteringWithinCurrentTag && !d->filter->isEmpty();

    PkVector<KisResourceRecord> source;
    if (!ignoreTagFiltering && d->currentTagFilter &&
        d->currentTagFilter->url() != KisAllTagsModel::urlAll() &&
        d->currentTagFilter->url() != KisAllTagsModel::urlAllUntagged()) {
        d->tagResourceModel->setTagsFilter(
            PkVector<KisTagSP>{d->currentTagFilter});
        source = d->tagResourceModel->records();
    } else {
        d->tagResourceModel->setTagsFilter(PkVector<KisTagSP>());
        source = d->resourceModel->records();
    }

    PkVector<KisResourceRecord> result;
    for (const KisResourceRecord &record : source) {
        if (d->currentResourceFilter &&
            record.id != d->currentResourceFilter->resourceId()) {
            continue;
        }
        if (!ignoreTagFiltering && d->currentTagFilter &&
            d->currentTagFilter->url() == KisAllTagsModel::urlAllUntagged() &&
            !record.tags.isEmpty()) {
            continue;
        }
        result.append(record);
    }
    return result;
}

bool KisTagFilterResourceProxyModel::accepts(
    const KisResourceRecord &record) const
{
    if (d->useStorageIdFilter && record.storageId != d->storageId) {
        return false;
    }

    KisResourceMetaDataModel *metaDataModel =
        KisResourceModelProvider::resourceMetadataModel();
    for (const PkString &key : d->metaDataMapFilter.keys()) {
        const PkVariant value = metaDataModel->metaDataValue(record.id, key);
        if (value.isValid() && value != d->metaDataMapFilter.value(key)) {
            return false;
        }
    }

    PkString resourceName = record.name;
    if (record.resourceType == ResourceType::PaintOpPresets) {
        const std::string source = resourceName.PkToUtf8();
        std::string normalized = source;
        for (char &character : normalized) {
            if (character == '_') {
                character = ' ';
            }
        }
        resourceName = PkString::PkFromUtf8(normalized.data(),
                                            static_cast<int>(normalized.size()));
    }

    if (d->filter->matchesResource(resourceName, record.tags)) {
        return true;
    }
    return additionalResourceNameChecks(record, d->filter);
}

PkVector<KisResourceRecord> KisTagFilterResourceProxyModel::records() const
{
    PkVector<KisResourceRecord> result;
    for (const KisResourceRecord &record : sourceRecords()) {
        if (accepts(record)) {
            result.append(record);
        }
    }
    return result;
}

PkVector<KoResourceSP> KisTagFilterResourceProxyModel::resources() const
{
    PkVector<KoResourceSP> result;
    for (const KisResourceRecord &record : records()) {
        KoResourceSP resource = d->resourceModel->resourceForId(record.id);
        if (resource) {
            result.append(resource);
        }
    }
    return result;
}

KoResourceSP KisTagFilterResourceProxyModel::resourceForId(int resourceId) const
{
    for (const KisResourceRecord &record : records()) {
        if (record.id == resourceId) {
            return d->resourceModel->resourceForId(resourceId);
        }
    }
    return KoResourceSP();
}

bool KisTagFilterResourceProxyModel::setResourceActive(int resourceId,
                                                       bool value)
{
    return d->resourceModel->setResourceActive(resourceId, value);
}

KoResourceSP KisTagFilterResourceProxyModel::importResourceFile(
    const PkString &filename,
    bool allowOverwrite,
    const PkString &storageId)
{
    return d->resourceModel->importResourceFile(filename,
                                                allowOverwrite,
                                                storageId);
}

KoResourceSP KisTagFilterResourceProxyModel::importResource(
    const PkString &filename,
    PkStream *device,
    bool allowOverwrite,
    const PkString &storageId)
{
    return d->resourceModel->importResource(filename,
                                            device,
                                            allowOverwrite,
                                            storageId);
}

bool KisTagFilterResourceProxyModel::importWillOverwriteResource(
    const PkString &filename,
    const PkString &storageLocation) const
{
    return d->resourceModel->importWillOverwriteResource(filename,
                                                         storageLocation);
}

bool KisTagFilterResourceProxyModel::exportResource(KoResourceSP resource,
                                                     PkStream *device)
{
    return d->resourceModel->exportResource(resource, device);
}

bool KisTagFilterResourceProxyModel::addResource(KoResourceSP resource,
                                                 const PkString &storageId)
{
    return d->resourceModel->addResource(resource, storageId);
}

bool KisTagFilterResourceProxyModel::addResourceDeduplicateFileName(
    KoResourceSP resource,
    const PkString &storageId)
{
    return d->resourceModel->addResourceDeduplicateFileName(resource, storageId);
}

bool KisTagFilterResourceProxyModel::updateResource(KoResourceSP resource)
{
    return d->resourceModel->updateResource(resource);
}

bool KisTagFilterResourceProxyModel::reloadResource(KoResourceSP resource)
{
    return d->resourceModel->reloadResource(resource);
}

bool KisTagFilterResourceProxyModel::renameResource(KoResourceSP resource,
                                                    const PkString &name)
{
    return d->resourceModel->renameResource(resource, name);
}

bool KisTagFilterResourceProxyModel::setResourceMetaData(
    KoResourceSP resource,
    PkMap<PkString, PkVariant> metadata)
{
    return d->resourceModel->setResourceMetaData(resource, metadata);
}

bool KisTagFilterResourceProxyModel::additionalResourceNameChecks(
    const KisResourceRecord &record,
    const KisResourceSearchBoxFilter *filter) const
{
    (void)record;
    (void)filter;
    return false;
}

void KisTagFilterResourceProxyModel::setMetaDataFilter(
    const PkMap<PkString, PkVariant> &metaDataMap)
{
    d->metaDataMapFilter = metaDataMap;
}

void KisTagFilterResourceProxyModel::setTagFilter(const KisTagSP &tag)
{
    d->currentTagFilter = tag;
}

KisTagSP KisTagFilterResourceProxyModel::currentTagFilter() const
{
    return d->currentTagFilter;
}

void KisTagFilterResourceProxyModel::setStorageFilter(bool useFilter,
                                                      int storageId)
{
    d->useStorageIdFilter = useFilter;
    if (useFilter) {
        d->storageId = storageId;
    }
}

void KisTagFilterResourceProxyModel::setResourceFilter(
    const KoResourceSP &resource)
{
    d->currentResourceFilter = resource;
}

void KisTagFilterResourceProxyModel::setSearchText(
    const PkString &searchText)
{
    d->filter->setFilter(searchText);
}

void KisTagFilterResourceProxyModel::setFilterInCurrentTag(
    bool filterInCurrentTag)
{
    d->filteringWithinCurrentTag = filterInCurrentTag;
}

bool KisTagFilterResourceProxyModel::filterInCurrentTag() const
{
    return d->filteringWithinCurrentTag;
}

bool KisTagFilterResourceProxyModel::tagResources(
    const KisTagSP &tag,
    const PkVector<int> &resourceIds)
{
    return d->tagResourceModel->tagResources(tag, resourceIds);
}

bool KisTagFilterResourceProxyModel::untagResources(
    const KisTagSP &tag,
    const PkVector<int> &resourceIds)
{
    return d->tagResourceModel->untagResources(tag, resourceIds);
}

int KisTagFilterResourceProxyModel::isResourceTagged(const KisTagSP &tag,
                                                     int resourceId)
{
    return d->tagResourceModel->isResourceTagged(tag, resourceId);
}
