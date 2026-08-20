/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KisFolderStorage.h"

#include <KisMimeDatabase.h>
#include <KisTag.h>
#include <KisResourceLoaderRegistry.h>
#include <KisGlobalResourcesInterface.h>
#include <KoMD5Generator.h>
#include <PkFileStream.h>
#include "PkResourceStorageDesktop.h"
#include "KisResourceThumbnailCodec.h"
#include "ResourceDebug.h"

#include <filesystem>

namespace {

std::filesystem::path resourcePath(const PkString &path)
{
    return std::filesystem::u8path(path.PkToUtf8());
}

PkString resourcePathString(const std::filesystem::path &path)
{
    const std::string utf8 = path.u8string();
    return PkString::PkFromUtf8(utf8.data(), static_cast<int>(utf8.size()));
}

}


class FolderTagIterator : public KisResourceStorage::TagIterator
{
public:

    FolderTagIterator(const PkString &location, const PkString &resourceType)
        : m_location(location)
        , m_resourceType(resourceType)
    {
        m_dirIterator = m_storage.listEntries(location + "/" + resourceType,
                                              {PkString("*.tag")},
                                              PkResourceStorage::EntryKind::Files, true);
    }

    bool hasNext() const override
    {
        return m_dirIterator->hasNext();
    }

    void next() override
    {
        m_dirIterator->next();
        const_cast<FolderTagIterator*>(this)->m_tag.reset(new KisTag);
        if (!load(m_tag)) {
            qCWarning(RESOURCE_LOG) << "Could not load tag" << m_dirIterator->url();
        }
    }

    KisTagSP tag() const override
    {
        return m_tag;
    }

private:

    bool load(KisTagSP tag) const
    {
        PkFileStream f(m_dirIterator->url());
        tag->setFilename(resourcePathString(resourcePath(m_dirIterator->url()).filename()));
        if (f.open(PkStream::ReadOnly)) {
            if (!tag->load(f)) {
                qCWarning(RESOURCE_LOG) << m_dirIterator->url() << "is not a valid tag desktop file";
                return false;
            }

        }
        return true;
    }

    PkResourceStorageDesktop m_storage;
    std::unique_ptr<PkResourceStorage::EntryIterator> m_dirIterator;
    PkString m_location;
    PkString m_resourceType;
    KisTagSP m_tag;
};


class FolderItem : public KisResourceStorage::ResourceItem
{
public:
    ~FolderItem() override {}
};


KisFolderStorage::KisFolderStorage(const PkString &location)
    : KisStoragePlugin(location)
{
}

KisFolderStorage::~KisFolderStorage()
{
}

bool KisFolderStorage::saveAsNewVersion(const PkString &resourceType, KoResourceSP _resource)
{
    return KisStorageVersioningHelper::addVersionedResource(location() + "/" + resourceType, _resource, 0);
}

KisResourceStorage::ResourceItem KisFolderStorage::resourceItem(const PkString &url)
{
    const std::filesystem::path native = resourcePath(url);
    FolderItem item;
    item.url = url;
    item.folder = resourcePathString(native.parent_path().filename());
    PkResourceStorageDesktop storage;
    const int64_t modified = storage.lastModified(url);
    if (modified) item.lastModified = PkDateTime::fromMSecsSinceEpoch(modified);
    return item;
}

bool KisFolderStorage::loadVersionedResource(KoResourceSP resource)
{
    const PkString path = location() + "/" + resource->resourceType().first + "/" + resource->filename();
    PkFileStream f(path);
    if (!f.open(PkStream::ReadOnly)) {
        qCWarning(RESOURCE_LOG) << "Could not open" << path << "for reading";
        return false;
    }

    bool r = resource->loadFromDevice(&f, KisGlobalResourcesInterface::instance());

    // Check for the thumbnail
    if (r) {
        sanitizeResourceFileNameCase(resource,
                                     location() + "/" + resource->resourceType().first);
        if ((resource->image().isNull() || resource->thumbnail().isNull()) &&
            !resource->thumbnailPath().isEmpty()) {
            const PkString thumbnailPath = PkResourceStorage::joinPath(
                location() + "/" + resource->resourceType().first,
                resource->thumbnailPath());
            const PkImage thumbnail = KisResourceThumbnailCodec::loadPng(thumbnailPath);
            if (!thumbnail.isNull()) {
                resource->setImage(thumbnail);
                resource->updateThumbnail();
            }
        }
    }

    return r;
}

PkString KisFolderStorage::resourceMd5(const PkString &url)
{
    PkString result;
    PkFileStream file(location() + "/" + url);
    if (PkResourceStorageDesktop().exists(location() + "/" + url) && file.open(PkStream::ReadOnly)) {
        result = KoMD5Generator::generateHash(&file);
    }

    return result;
}

PkString KisFolderStorage::resourceFilePath(const PkString &url)
{
    const PkString path = location() + "/" + url;
    PkResourceStorageDesktop storage;
    return storage.exists(path) ? storage.absolutePath(path) : PkString();
}

PkSharedPointer<KisResourceStorage::ResourceIterator> KisFolderStorage::resources(const PkString &resourceType)
{
    PkVector<VersionedResourceEntry> entries;

    const PkString resourcesSaveLocation = location() + "/" + resourceType;

    std::vector<PkString> filters;
    for (const PkString &filter : KisResourceLoaderRegistry::instance()->filters(resourceType)) filters.push_back(filter);
    PkResourceStorageDesktop storage;
    auto it = storage.listEntries(resourcesSaveLocation, filters, PkResourceStorage::EntryKind::Files, true);

    while (it->hasNext()) {
        it->next();
        VersionedResourceEntry entry;
        entry.filename = it->url().mid(resourcesSaveLocation.size() + 1);

        // Don't load 4.x backup resources
        if (entry.filename.contains("backup")) {
            continue;
        }

        entry.lastModified = PkDateTime::fromMSecsSinceEpoch(it->lastModified());
        entry.tagList = {}; // TODO
        entry.resourceType = resourceType;
        entries.append(entry);
    }

    KisStorageVersioningHelper::detectFileVersions(entries);

    return PkSharedPointer<KisResourceStorage::ResourceIterator>(new KisVersionedStorageIterator(entries, this));
}

PkSharedPointer<KisResourceStorage::TagIterator> KisFolderStorage::tags(const PkString &resourceType)
{
    return PkSharedPointer<KisResourceStorage::TagIterator>(new FolderTagIterator(location(), resourceType));
}

bool KisFolderStorage::importResource(const PkString &url, PkStream *device)
{
    bool result = false;

    const PkString resourcesLocation = location() + "/" + url;
    PkResourceStorageDesktop storage;
    PkFileStream f(resourcesLocation);
    if (storage.exists(resourcesLocation)) return result;

    if (f.open(PkStream::WriteOnly | PkStream::Truncate)) {
        PkStream::pk_int64 writtenBytes = 0;
        char buffer[8192];
        for (PkStream::pk_int64 n; (n = device->read(buffer, sizeof(buffer))) > 0;) writtenBytes += f.write(buffer, n);
        f.close();
        result = (writtenBytes == device->size());
    } else {
        qCWarning(RESOURCE_LOG) << "Cannot open" << resourcesLocation << "for writing";
    }

    KoResourceSP resourceAfterLoading = resource(url);

    if (resourceAfterLoading.isNull()) {
        storage.remove(resourcesLocation);
        return false;
    }

    return result;
}

bool KisFolderStorage::exportResource(const PkString &url, PkStream *device)
{
    bool result = false;

    const PkString resourcesLocation = location() + "/" + url;
    PkFileStream f(resourcesLocation);
    if (!PkResourceStorageDesktop().exists(resourcesLocation)) return result;

    if (f.open(PkStream::ReadOnly)) {
        char buffer[8192];
        for (PkStream::pk_int64 n; (n = f.read(buffer, sizeof(buffer))) > 0;) device->write(buffer, n);
        f.close();
        result = true;
    } else {
        qCWarning(RESOURCE_LOG) << "Cannot open" << resourcesLocation << "for reading";
    }

    return result;
}

bool KisFolderStorage::addResource(const PkString &resourceType, KoResourceSP resource)
{
    if (!resource || !resource->valid()) return false;

    const PkString resourcesSaveLocation = location() + "/" + resourceType;

    const PkString path = resourcesSaveLocation + "/" + resource->filename();
    if (PkResourceStorageDesktop().exists(path)) {
        qCWarning(RESOURCE_LOG) << "Resource" << resourceType << resource->filename() << "already exists in" << resourcesSaveLocation;
        return false;
    }

    PkFileStream resourceFile(path);
    if (!resourceFile.open(PkStream::WriteOnly | PkStream::Truncate)) {
        qCWarning(RESOURCE_LOG) << "Could not open" << path << "for writing.";
        return false;
    }

    if (!resource->saveToDevice(&resourceFile)) {
        qCWarning(RESOURCE_LOG) << "Could not save resource to" << path;
        resourceFile.close();
        return false;
    }
    resourceFile.close();




    return true;
}

PkStringList KisFolderStorage::metaDataKeys() const
{
    return PkStringList() << KisResourceStorage::s_meta_name;
}

PkVariant KisFolderStorage::metaData(const PkString &key) const
{
    if (key == KisResourceStorage::s_meta_name) {
        return PkString("Local Resources");
    }
    return PkVariant();

}
