/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KisStoragePlugin.h"
#include "PkResourceStorageDesktop.h"
#include "ResourceDebug.h"

#include <KoResource.h>
#include <KisMimeDatabase.h>
#include <KisResourceLoaderRegistry.h>

class KisStoragePlugin::Private
{
public:
    PkString location;
    PkDateTime timestamp;
    PkResourceStorageDesktop storage;
};

KisStoragePlugin::KisStoragePlugin(const PkString &location)
    : d(new Private())
{
    d->location = location;

    if (!d->storage.exists(d->location)) {
        d->timestamp = PkDateTime::currentDateTime();
    }
}

KisStoragePlugin::~KisStoragePlugin()
{
}

KoResourceSP KisStoragePlugin::resource(const PkString &url)
{
    if (!url.contains("/")) return nullptr;

    const std::string text = url.PkToUtf8();
    const std::size_t separator = text.find('/');
    if (separator == std::string::npos || separator == 0 || separator + 1 >= text.size()) return nullptr;
    const PkString resourceType = PkString::PkFromUtf8(text.data(), static_cast<int>(separator));
    const PkString resourceFilename = PkString::PkFromUtf8(
        text.data() + separator + 1, static_cast<int>(text.size() - separator - 1));

    const PkString mime = KisMimeDatabase::mimeTypeForFile(resourceFilename);

    KisResourceLoaderBase *loader = KisResourceLoaderRegistry::instance()->loader(resourceType, mime);
    if (!loader) {
        qCWarning(RESOURCE_LOG) << "Could not create loader for" << resourceType << resourceFilename << mime;
        return nullptr;
    }

    KoResourceSP resource = loader->create(resourceFilename);
    return loadVersionedResource(resource) ? resource : nullptr;
}

PkString KisStoragePlugin::resourceMd5(const PkString &url)
{
    // a fallback implementation for the storages with
    // ephemeral resources
    KoResourceSP res = resource(url);
    if (res) {
        return res->md5Sum();
    } else {
        return PkString();
    }
}

PkString KisStoragePlugin::resourceFilePath(const PkString &)
{
    return PkString();
}

bool KisStoragePlugin::supportsVersioning() const
{
    return true;
}

PkDateTime KisStoragePlugin::timestamp()
{
    if (d->timestamp.isNull()) {
        const int64_t modified = d->storage.lastModified(d->location);
        return modified ? PkDateTime::fromMSecsSinceEpoch(modified) : PkDateTime();
    }
    return d->timestamp;
}

PkString KisStoragePlugin::location() const
{
    return d->location;
}

void KisStoragePlugin::sanitizeResourceFileNameCase(KoResourceSP resource, const PkString &parentDir)
{
    auto result = d->storage.listEntries(parentDir, {resource->filename()},
                                         PkResourceStorage::EntryKind::Files, false);
    if (result->hasNext()) {
        result->next();
        const std::string path = result->url().PkToUtf8();
        const std::size_t separator = path.find_last_of("/\\");
        const std::size_t nameOffset = separator == std::string::npos ? 0 : separator + 1;
        const PkString realName = PkString::PkFromUtf8(
            path.data() + nameOffset, static_cast<int>(path.size() - nameOffset));
        if (realName != resource->filename()) {
            resource->setFilename(realName);
        }
    }
}

bool KisStoragePlugin::isValid() const
{
    qCWarning(RESOURCE_LOG) << "Storage plugins should implement their own checks!";
    return true;
}
