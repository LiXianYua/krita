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
    record.thumbnail = thumbnailFromResourceQuery(query, useResourcePrefix);

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
