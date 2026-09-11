/*
 * SPDX-FileCopyrightText: 2020 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-FileCopyrightText: 2021 Agata Cacko <cacko.azh@gmail.com>
 * SPDX-FileCopyrightText: 2022 Dmitry Kazakov <dimula73@gmail.com>
 * SPDX-FileCopyrightText: 2023 L. E. Segovia <amy@amyspark.me>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "KisResourceQueryMapper.h"

#include <PkSqlQuery.h>

#include "KisResourceLocator.h"
#include "KisResourceThumbnailCache.h"
#include "KisResourceThumbnailCodec.h"

namespace
{

PkString columnName(bool prefixed, const char *plain, const char *withPrefix)
{
    return PkString(prefixed ? withPrefix : plain);
}

} // namespace

PkImage KisResourceQueryMapper::thumbnailFromResourceQuery(const PkSqlQuery &query,
                                                           bool useResourcePrefix)
{
    KisResourceLocator *locator = KisResourceLocator::instance();
    KisResourceThumbnailCache *cache = KisResourceThumbnailCache::instance();
    if (!locator || !cache) {
        return PkImage();
    }

    const PkString storageLocation = locator->makeStorageLocationAbsolute(
        query.value(PkString("location")).toString());
    const PkString resourceType = query.value(PkString("resource_type")).toString();
    const PkString filename = query.value(columnName(useResourcePrefix,
                                                     "filename",
                                                     "resource_filename")).toString();

    PkImage image = cache->originalImage(storageLocation, resourceType, filename);
    if (!image.isNull()) {
        return image;
    }

    const int resourceId = query.value(columnName(useResourcePrefix,
                                                   "id",
                                                   "resource_id")).toInt();
    if (resourceId < 0) {
        return PkImage();
    }

    PkSqlQuery thumbnailQuery;
    if (!thumbnailQuery.prepare(PkString(
            "SELECT thumbnail FROM resources WHERE id = :resource_id"))) {
        return PkImage();
    }
    thumbnailQuery.bindValue(PkString(":resource_id"), PkVariant(resourceId));
    if (!thumbnailQuery.exec() || !thumbnailQuery.first()) {
        return PkImage();
    }

    image = KisResourceThumbnailCodec::decodePng(
        thumbnailQuery.value(PkString("thumbnail")).toByteArray());
    if (!image.isNull()) {
        cache->insert(storageLocation, resourceType, filename, image);
    }
    return image;
}

KisResourceRecord KisResourceQueryMapper::resourceFromQuery(const PkSqlQuery &query,
                                                            bool useResourcePrefix)
{
    KisResourceRecord record;
    record.id = query.value(columnName(useResourcePrefix, "id", "resource_id")).toInt();
    record.storageId = query.value(PkString("storage_id")).toInt();
    record.name = query.value(columnName(useResourcePrefix,
                                         "name",
                                         "resource_name")).toString();
    record.filename = query.value(columnName(useResourcePrefix,
                                             "filename",
                                             "resource_filename")).toString();
    record.tooltip = query.value(columnName(useResourcePrefix,
                                            "tooltip",
                                            "resource_tooltip")).toString();
    record.status = query.value(columnName(useResourcePrefix,
                                           "status",
                                           "resource_active")).toBool();
    record.location = query.value(PkString("location")).toString();
    record.resourceType = query.value(PkString("resource_type")).toString();
    record.md5 = query.value(columnName(useResourcePrefix,
                                       "md5sum",
                                       "resource_md5sum")).toString();
    record.resourceActive = query.value(PkString("resource_active")).toBool();
    record.storageActive = query.value(columnName(useResourcePrefix,
                                                  "storage_active",
                                                  "resource_storage_active")).toBool();

    KisResourceLocator *locator = KisResourceLocator::instance();
    if (locator && record.id >= 0) {
        record.metaData = locator->metaDataForResource(record.id);
        if (locator->resourceCached(record.location,
                                    record.resourceType,
                                    record.filename)) {
            KoResourceSP resource = locator->resourceForId(record.id);
            record.dirty = resource && resource->isDirty();
        }
    }

    return record;
}

PkMap<int, PkImage> KisResourceQueryMapper::thumbnailsForRequests(
    const PkVector<ThumbnailRequest> &requests)
{
    PkMap<int, PkImage> images;

    KisResourceLocator *locator = KisResourceLocator::instance();
    KisResourceThumbnailCache *cache = KisResourceThumbnailCache::instance();
    if (!locator || !cache) {
        return images;
    }

    // Serve whatever the in-memory cache already holds, and collect the ids
    // that still need a database round-trip. One resource can legitimately
    // appear more than once in a single batch (the tag model lists a resource
    // once per tag), so the id list is de-duplicated before it becomes an
    // `IN (...)`.
    PkVector<int> missingIds;
    PkMap<int, bool> seenIds;
    for (const ThumbnailRequest &request : requests) {
        const PkImage cached = cache->originalImage(request.storageLocation,
                                                    request.resourceType,
                                                    request.filename);
        if (!cached.isNull()) {
            images.insert(request.resourceId, cached);
            continue;
        }
        if (request.resourceId < 0 || seenIds.contains(request.resourceId)) {
            continue;
        }
        seenIds.insert(request.resourceId, true);
        missingIds.append(request.resourceId);
    }

    if (missingIds.isEmpty()) {
        return images;
    }

    PkString placeholders;
    for (int i = 0; i < missingIds.size(); ++i) {
        if (i > 0) {
            placeholders += PkString(",");
        }
        placeholders += PkString("?");
    }

    PkSqlQuery thumbnailQuery;
    if (!thumbnailQuery.prepare(PkString("SELECT id, thumbnail FROM resources "
                                         "WHERE id IN (")
                                + placeholders + PkString(")"))) {
        return images;
    }
    for (const int id : missingIds) {
        thumbnailQuery.addBindValue(PkVariant(id));
    }
    if (!thumbnailQuery.exec()) {
        return images;
    }

    PkMap<int, PkImage> decoded;
    while (thumbnailQuery.next()) {
        const PkImage image = KisResourceThumbnailCodec::decodePng(
            thumbnailQuery.value(PkString("thumbnail")).toByteArray());
        if (!image.isNull()) {
            decoded.insert(thumbnailQuery.value(PkString("id")).toInt(), image);
        }
    }

    // Cache through the same insert() the single-row path uses, so that a null
    // image never becomes a cache entry on this path either.
    for (const ThumbnailRequest &request : requests) {
        const PkImage image = decoded.value(request.resourceId);
        if (image.isNull()) {
            continue;
        }
        if (cache->originalImage(request.storageLocation,
                                 request.resourceType,
                                 request.filename).isNull()) {
            cache->insert(request.storageLocation,
                          request.resourceType,
                          request.filename,
                          image);
        }
        images.insert(request.resourceId, image);
    }

    return images;
}
