/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KisMemoryStorage.h"

#include <optional>
#include <KisMimeDatabase.h>
#include <KisTag.h>
#include <KisResourceStorage.h>
#include <KisGlobalResourcesInterface.h>
#include <KoMD5Generator.h>
#include <kis_assert.h>
#include <PkMemoryStream.h>
#include <PkHash.h>
#include "ResourceDebug.h"


namespace detail {

    /**
     * TODO: move this function into KisStorageVersioningHelper with fixing the
     * versioning functions to handle subfolders as well
     */
    std::optional<std::pair<PkString, PkString>> splitResourceUrl(const PkString &url)
    {
        if (!url.contains("/")) return std::nullopt;

        const std::string text = url.PkToUtf8();
        const std::size_t separator = text.find('/');
        if (separator == std::string::npos || separator == 0 || separator + 1 >= text.size()) return std::nullopt;
        const PkString resourceType = PkString::PkFromUtf8(text.data(), static_cast<int>(separator));
        const PkString resourceFilename = PkString::PkFromUtf8(
            text.data() + separator + 1, static_cast<int>(text.size() - separator - 1));
        return std::make_pair(resourceType, resourceFilename);
    }

}

struct StoredResource
{
    PkDateTime timestamp;
    PkSharedPointer<PkMemoryStream> data;
    KoResourceSP resource;
};

class MemoryTagIterator : public KisResourceStorage::TagIterator
{
public:
    MemoryTagIterator(const PkVector<KisTagSP> &tags)
        : m_tags(tags)
    {
    }

    bool hasNext() const override
    {
        return m_index + 1 < m_tags.size();
    }

    void next() override
    {
        ++m_index;
    }

    KisTagSP tag() const override
    {
        return m_tags.at(m_index);
    }

private:
    PkVector<KisTagSP> m_tags;
    int m_index = -1;
};


class MemoryItem : public KisResourceStorage::ResourceItem
{
public:
    ~MemoryItem() override {}
};


class KisMemoryStorage::Private {
public:
    PkHash<PkString, PkHash<PkString, StoredResource>> resourcesNew;
    PkHash<PkString, PkVector<KisTagSP>> tags;
    PkMap<PkString, PkVariant> metadata;
};


KisMemoryStorage::KisMemoryStorage(const PkString &location)
    : KisStoragePlugin(location)
    , d(new Private)
{
}

KisMemoryStorage::~KisMemoryStorage()
{
}

KisMemoryStorage::KisMemoryStorage(const KisMemoryStorage &rhs)
    : KisStoragePlugin(rhs.location())
    , d(new Private)
{
    *this = rhs;
    d->resourcesNew = rhs.d->resourcesNew;
    d->tags = rhs.d->tags;
    d->metadata = rhs.d->metadata;
}

KisMemoryStorage &KisMemoryStorage::operator=(const KisMemoryStorage &rhs)
{
    if (this != &rhs) {
        d->resourcesNew = rhs.d->resourcesNew;

        for (const PkString &key : rhs.d->tags.keys()) {
            for (const KisTagSP &tag : rhs.d->tags[key]) {
                if (!d->tags.contains(key)) {
                    d->tags[key] = PkVector<KisTagSP>();
                }
                d->tags[key] << tag->clone();
            }
        }
    }
    return *this;
}

bool KisMemoryStorage::saveAsNewVersion(const PkString &resourceType, KoResourceSP resource)
{
    PkHash<PkString, StoredResource> &typedResources =
        d->resourcesNew[resourceType];

    auto checkExists =
        [&typedResources] (const PkString &filename) {
            return typedResources.contains(filename);
        };

    const PkString newFilename =
        KisStorageVersioningHelper::chooseUniqueName(resource, 0, checkExists);

    if (newFilename.isEmpty()) return false;

    resource->setFilename(newFilename);

    StoredResource storedResource;
    storedResource.timestamp = PkDateTime::currentDateTime();
    storedResource.data.reset(new PkMemoryStream());
    storedResource.data->open(PkStream::WriteOnly | PkStream::Truncate);
    bool result = resource->saveToDevice(storedResource.data.data());
    storedResource.data->close();
    if (!result) {
        storedResource.resource = resource;
    }

    typedResources.insert(newFilename, storedResource);

    return true;
}

KisResourceStorage::ResourceItem KisMemoryStorage::resourceItem(const PkString &url)
{
    MemoryItem item;
    item.url = url;
    item.folder = PkString();
    item.lastModified = PkDateTime::fromMSecsSinceEpoch(0);
    return item;
}

bool KisMemoryStorage::loadVersionedResource(KoResourceSP resource)
{
    const PkString resourceType = resource->resourceType().first;
    const PkString resourceFilename = resource->filename();

    bool retval = false;

    if (d->resourcesNew.contains(resourceType) &&
        d->resourcesNew[resourceType].contains(resourceFilename)) {

        const StoredResource &storedResource =
            d->resourcesNew[resourceType][resourceFilename];

        if (storedResource.data->size() > 0) {
            storedResource.data->close();
            storedResource.data->open(PkStream::ReadOnly);
            resource->loadFromDevice(storedResource.data.data(), KisGlobalResourcesInterface::instance());
            storedResource.data->close();
        } else {
            KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(storedResource.data->size() > 0, false);
            qCWarning(RESOURCE_LOG) << "Cannot load resource from device in KisMemoryStorage::loadVersionedResource";
            return false;
        }
        retval = true;
    }

    return retval;
}

bool KisMemoryStorage::importResource(const PkString &url, PkStream *device)
{
    auto parsedUrl = detail::splitResourceUrl(url);
    if (!parsedUrl) return false;
    auto [resourceType, resourceFilename] = *parsedUrl;

    // we cannot overwrite existing file by API convention
    if (d->resourcesNew.contains(resourceType) &&
        d->resourcesNew[resourceType].contains(resourceFilename)) {
        return false;
    }

    StoredResource storedResource;
    storedResource.timestamp = PkDateTime::currentDateTime();
    storedResource.data.reset(new PkMemoryStream());
    storedResource.data->open(PkStream::WriteOnly | PkStream::Truncate);
    char buffer[8192];
    for (PkStream::pk_int64 n; (n = device->read(buffer, sizeof(buffer))) > 0;) storedResource.data->write(buffer, n);
    storedResource.data->close();

    PkHash<PkString, StoredResource> &typedResources =
        d->resourcesNew[resourceType];
    typedResources.insert(resourceFilename, storedResource);

    return true;
}

bool KisMemoryStorage::exportResource(const PkString &url, PkStream *device)
{
    auto parsedUrl = detail::splitResourceUrl(url);
    if (!parsedUrl) return false;
    auto [resourceType, resourceFilename] = *parsedUrl;

    if (!d->resourcesNew.contains(resourceType) ||
        !d->resourcesNew[resourceType].contains(resourceFilename)) {
        return false;
    }

    const StoredResource &storedResource =
        d->resourcesNew[resourceType][resourceFilename];

    if (!storedResource.data) {
        qCWarning(RESOURCE_LOG) << "Stored resource doesn't have a serialized representation!";
        return false;
    }

    storedResource.data->close();
    storedResource.data->open(PkStream::ReadOnly);
    char buffer[8192];
    for (PkStream::pk_int64 n; (n = storedResource.data->read(buffer, sizeof(buffer))) > 0;) device->write(buffer, n);
    storedResource.data->close();
    return true;
}

bool KisMemoryStorage::addResource(const PkString &resourceType,  KoResourceSP resource)
{
    PkHash<PkString, StoredResource> &typedResources = d->resourcesNew[resourceType];

    if (typedResources.contains(resource->filename())) {
        /// here we silently overwrite the resource if the filename
        /// is the same; it is the job of higher-level code to actually
        /// resolve file name clashes
        return true;
    };

    StoredResource storedResource;
    storedResource.timestamp = PkDateTime::currentDateTime();
    storedResource.data.reset(new PkMemoryStream());
    if (resource->isSerializable()) {
        storedResource.data->open(PkStream::WriteOnly | PkStream::Truncate);
        if (!resource->saveToDevice(storedResource.data.data())) {
            storedResource.resource = resource;
        }
        storedResource.data->close();
    } else {
        storedResource.resource = resource;
    }

    typedResources.insert(resource->filename(), storedResource);

    return true;
}

bool KisMemoryStorage::testingRemoveResource(const PkString &url)
{
    auto parsedUrl = detail::splitResourceUrl(url);
    if (!parsedUrl) return false;
    auto [resourceType, resourceFilename] = *parsedUrl;

    if (d->resourcesNew.contains(resourceType)) {
        return d->resourcesNew[resourceType].remove(resourceFilename) > 0;
    }

    return false;
}

bool KisMemoryStorage::testingAddTag(const PkString &resourceType, KisTagSP tag)
{
    KIS_SAFE_ASSERT_RECOVER_NOOP(resourceType == tag->resourceType());

    PkVector<KisTagSP> &typedTags = d->tags[resourceType];

    auto existingIt = std::find_if(typedTags.begin(), typedTags.end(), [&tag](const KisTagSP &value) {
        return value->url() == tag->url();
    });
    if (existingIt != typedTags.end()) {
        typedTags.erase(existingIt);
    }

    typedTags.append(tag);

    return true;
}

bool KisMemoryStorage::testingRemoveTag(const PkString &resourceType, const PkString &tagUrl)
{
    PkVector<KisTagSP> &typedTags = d->tags[resourceType];

    auto existingIt = std::find_if(typedTags.begin(), typedTags.end(), [&tagUrl](const KisTagSP &value) {
        return value->url() == tagUrl;
    });
    if (existingIt != typedTags.end()) {
        typedTags.erase(existingIt);
        return true;
    }

    return false;
}

PkString KisMemoryStorage::resourceMd5(const PkString &url)
{
    auto parsedUrl = detail::splitResourceUrl(url);
    if (!parsedUrl) return PkString();
    auto [resourceType, resourceFilename] = *parsedUrl;

    PkString result;

    if (d->resourcesNew.contains(resourceType) &&
        d->resourcesNew[resourceType].contains(resourceFilename)) {

        const StoredResource &storedResource =
            d->resourcesNew[resourceType][resourceFilename];

        if (storedResource.data->size() > 0 || storedResource.resource.isNull()) {
            storedResource.data->close();
            storedResource.data->open(PkStream::ReadOnly);
            result = KoMD5Generator::generateHash(storedResource.data.data());
            storedResource.data->close();
        } else {
            result = storedResource.resource->md5Sum();
        }
    }

    return result;
}

PkSharedPointer<KisResourceStorage::ResourceIterator> KisMemoryStorage::resources(const PkString &resourceType)
{
    PkVector<VersionedResourceEntry> entries;


    PkHash<PkString, StoredResource> &typedResources =
        d->resourcesNew[resourceType];

    for (auto it = typedResources.begin(); it != typedResources.end(); ++it) {
        VersionedResourceEntry entry;
        entry.filename = it.key();
        entry.lastModified = it.value().timestamp;
        entry.tagList = {}; // TODO
        entry.resourceType = resourceType;
        entries.append(entry);

    }

    KisStorageVersioningHelper::detectFileVersions(entries);

    return PkSharedPointer<KisResourceStorage::ResourceIterator>(new KisVersionedStorageIterator(entries, this));
}

PkSharedPointer<KisResourceStorage::TagIterator> KisMemoryStorage::tags(const PkString &resourceType)
{
    return PkSharedPointer<KisResourceStorage::TagIterator>(new MemoryTagIterator(d->tags[resourceType]));
}

void KisMemoryStorage::setMetaData(const PkString &key, const PkVariant &value)
{
    d->metadata[key] = value;
}

PkStringList KisMemoryStorage::metaDataKeys() const
{
    PkStringList keys = d->metadata.keys();

    if (!keys.contains(KisResourceStorage::s_meta_name)) {
        keys << KisResourceStorage::s_meta_name;
    }

    return keys;
}

PkVariant KisMemoryStorage::metaData(const PkString &key) const
{
    PkVariant r;
    if (d->metadata.contains(key)) {
        r = d->metadata[key];
    }
    return r;
}
