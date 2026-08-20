/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KisBundleStorage.h"

#include <KisTag.h>
#include "KisResourceStorage.h"
#include <KoMD5Generator.h>
#include <KoResourceBundle.h>
#include <KoResourceBundleManifest.h>
#include <KisGlobalResourcesInterface.h>

#include <KisResourceLoaderRegistry.h>
#include <PkFileStream.h>
#include "PkResourceStorageDesktop.h"
#include "KisResourceThumbnailCodec.h"
#include "ResourceDebug.h"
#include <kis_assert.h>

#include <filesystem>

class KisBundleStorage::Private {
public:
    Private(KisBundleStorage *_q) : q(_q) {}

    KisBundleStorage *q;
    PkScopedPointer<KoResourceBundle> bundle;
};


class BundleTagIterator : public KisResourceStorage::TagIterator
{
public:

    BundleTagIterator(KoResourceBundle *bundle, const PkString &resourceType)
        : m_bundle(bundle)
        , m_resourceType(resourceType)
    {
        PkList<KoResourceBundleManifest::ResourceReference> resources = m_bundle->manifest().files(resourceType);
        for (const KoResourceBundleManifest::ResourceReference &resourceReference : resources) {
            for (const PkString &tagname : resourceReference.tagList) {
                if (!m_tags.contains(tagname)){
                    KisTagSP tag = PkSharedPointer<KisTag>(new KisTag());
                    tag->setName(tagname);
                    tag->setComment(tagname);
                    tag->setUrl(tagname);
                    tag->setResourceType(resourceType);
                    tag->setValid(true);
                    m_tags[tagname] = tag;
                }

                m_tags[tagname]->setDefaultResources(m_tags[tagname]->defaultResources()
                                                     << PkString(std::filesystem::path(resourceReference.resourcePath.PkToUtf8()).filename().string().c_str()));
            }
        }
        m_tagValues = m_tags.values();
    }

    bool hasNext() const override
    {
        return m_tagIndex + 1 < m_tagValues.size();
    }

    void next() override
    {
        m_tag = m_tagValues.at(++m_tagIndex);
    }
    KisTagSP tag() const override { return m_tag; }

private:
    PkMap<PkString, KisTagSP> m_tags;
    KoResourceBundle *m_bundle {0};
    PkString m_resourceType;
    PkList<KisTagSP> m_tagValues;
    int m_tagIndex = -1;
    KisTagSP m_tag;
};


KisBundleStorage::KisBundleStorage(const PkString &location)
    : KisStoragePlugin(location)
    , d(new Private(this))
{
    d->bundle.reset(new KoResourceBundle(location));
    if (!d->bundle->load()) {
        qCWarning(RESOURCE_LOG) << "Could not load bundle" << location;
    }
}

KisBundleStorage::~KisBundleStorage()
{
}

KisResourceStorage::ResourceItem KisBundleStorage::resourceItem(const PkString &url)
{
    KisResourceStorage::ResourceItem item;
    item.url = url;
    const std::vector<PkString> parts = url.split(u'/');
    KIS_ASSERT(parts.size() == 2);
    item.folder = parts[0];
    item.resourceType = parts[0];
    PkResourceStorageDesktop storage;
    const int64_t modified = storage.lastModified(d->bundle->filename());
    if (modified) item.lastModified = PkDateTime::fromMSecsSinceEpoch(modified);
    return item;
}

bool KisBundleStorage::loadVersionedResource(KoResourceSP resource)
{
    bool foundVersionedFile = false;

    const PkString resourceType = resource->resourceType().first;

    const PkString bundleSaveLocation = location() + "_modified" + "/" + resourceType;

    PkResourceStorageDesktop storage;
    if (storage.exists(bundleSaveLocation)) {
        const PkString fn = bundleSaveLocation  + "/" + resource->filename();
        if (storage.exists(fn)) {
            foundVersionedFile = true;

            PkFileStream f(fn);
            if (!f.open(PkStream::ReadOnly)) {
                qCWarning(RESOURCE_LOG) << "Could not open resource file for reading" << fn;
                return false;
            }
            if (!resource->loadFromDevice(&f, KisGlobalResourcesInterface::instance())) {
                qCWarning(RESOURCE_LOG) << "Could not reload resource file" << fn;
                return false;
            }

            sanitizeResourceFileNameCase(resource, bundleSaveLocation);
            if ((resource->image().isNull() || resource->thumbnail().isNull()) &&
                !resource->thumbnailPath().isEmpty()) {
                const PkString thumbnailPath = PkResourceStorage::joinPath(
                    bundleSaveLocation, resource->thumbnailPath());
                const PkImage thumbnail = KisResourceThumbnailCodec::loadPng(thumbnailPath);
                if (!thumbnail.isNull()) {
                    resource->setImage(thumbnail);
                    resource->updateThumbnail();
                }
            }
            f.close();
        }
    }

    if (!foundVersionedFile) {
        d->bundle->loadResource(resource);
    }

    return true;
}

PkString KisBundleStorage::resourceMd5(const PkString &url)
{
    PkString result;

    const PkString modifiedPath = location() + "_modified" + "/" + url;
    PkFileStream modifiedFile(modifiedPath);
    if (PkResourceStorageDesktop().exists(modifiedPath) && modifiedFile.open(PkStream::ReadOnly)) {
        result = KoMD5Generator::generateHash(&modifiedFile);
    } else {
        result = d->bundle->resourceMd5(url);
    }

    return result;
}

PkSharedPointer<KisResourceStorage::ResourceIterator> KisBundleStorage::resources(const PkString &resourceType)
{
    PkVector<VersionedResourceEntry> entries;

    PkList<KoResourceBundleManifest::ResourceReference> references =
        d->bundle->manifest().files(resourceType);

    for (auto it = references.begin(); it != references.end(); ++it) {
        VersionedResourceEntry entry;
        // it->resourcePath() contains paths like "brushes/ink.png" or "brushes/subfolder/splash.png".
        // we need to cut off the first part and get "ink.png" in the first case,
        // but "subfolder/splash.png" in the second case in order for subfolders to work
        // so it cannot just use a basename-only helper here.
        std::string path = it->resourcePath.PkToUtf8();
        std::replace(path.begin(), path.end(), '\\', '/');
        const std::size_t folderEndIdx = path.find('/');
        const PkString properFilenameWithSubfolders(
            (folderEndIdx == std::string::npos ? path : path.substr(folderEndIdx + 1)).c_str());

        entry.filename = properFilenameWithSubfolders;
        entry.lastModified = timestamp();
        entry.tagList = it->tagList;
        entry.resourceType = resourceType;
        entries.append(entry);
    }

    const PkString bundleSaveLocation = location() + "_modified" + "/" + resourceType;

    std::vector<PkString> filters;
    for (const PkString &filter : KisResourceLoaderRegistry::instance()->filters(resourceType)) filters.push_back(filter);
    PkResourceStorageDesktop storage;
    auto modifiedEntries = storage.listEntries(bundleSaveLocation, filters,
                                                PkResourceStorage::EntryKind::Files, true);

    while (modifiedEntries->hasNext()) {
        modifiedEntries->next();
        VersionedResourceEntry entry;
        entry.filename = PkString(std::filesystem::path(modifiedEntries->url().PkToUtf8()).filename().string().c_str());
        entry.lastModified = PkDateTime::fromMSecsSinceEpoch(modifiedEntries->lastModified());
        entry.tagList = {}; // TODO
        entry.resourceType = resourceType;
        entries.append(entry);
    }

    KisStorageVersioningHelper::detectFileVersions(entries);

    return PkSharedPointer<KisResourceStorage::ResourceIterator>(new KisVersionedStorageIterator(entries, this));
}

PkSharedPointer<KisResourceStorage::TagIterator> KisBundleStorage::tags(const PkString &resourceType)
{
    return PkSharedPointer<KisResourceStorage::TagIterator>(new BundleTagIterator(d->bundle.data(), resourceType));
}

PkImage KisBundleStorage::thumbnail() const
{
    return d->bundle->image();
}

PkStringList KisBundleStorage::metaDataKeys() const
{

    return PkStringList() << KisResourceStorage::s_meta_generator
                         << KisResourceStorage::s_meta_author
                         << KisResourceStorage::s_meta_title
                         << KisResourceStorage::s_meta_description
                         << KisResourceStorage::s_meta_initial_creator
                         << KisResourceStorage::s_meta_creator
                         << KisResourceStorage::s_meta_creation_date
                         << KisResourceStorage::s_meta_dc_date
                         << KisResourceStorage::s_meta_user_defined
                         << KisResourceStorage::s_meta_name
                         << KisResourceStorage::s_meta_value
                         << KisResourceStorage::s_meta_version;

}

PkVariant KisBundleStorage::metaData(const PkString &key) const
{
    return d->bundle->metaData(key);
}

bool KisBundleStorage::saveAsNewVersion(const PkString &resourceType, KoResourceSP resource)
{
    PkString bundleSaveLocation = location() + "_modified" + "/" + resourceType;

    if (!PkResourceStorageDesktop().exists(bundleSaveLocation)) {
        PkResourceStorageDesktop().mkpath(bundleSaveLocation);
    }

    return KisStorageVersioningHelper::addVersionedResource(bundleSaveLocation, resource, 1);
}

bool KisBundleStorage::exportResource(const PkString &url, PkStream *device)
{
    const std::vector<PkString> parts = url.split(u'/');
    KIS_ASSERT(parts.size() == 2);

    const PkString resourceType = parts[0];
    const PkString resourceFileName = parts[1];

    bool foundVersionedFile = false;

    const PkString bundleSaveLocation = location() + "_modified" + "/" + resourceType;

    PkResourceStorageDesktop storage;
    if (storage.exists(bundleSaveLocation)) {
        const PkString fn = bundleSaveLocation  + "/" + resourceFileName;
        if (storage.exists(fn)) {
            foundVersionedFile = true;

            PkFileStream f(fn);
            if (!f.open(PkStream::ReadOnly)) {
                qCWarning(RESOURCE_LOG) << "Could not open resource file for reading" << fn;
                return false;
            }

            char buffer[8192];
            for (PkStream::pk_int64 n; (n = f.read(buffer, sizeof(buffer))) > 0;) device->write(buffer, n);
        }
    }

    if (!foundVersionedFile) {
        d->bundle->exportResource(resourceType, resourceFileName, device);
    }

    return true;
}
