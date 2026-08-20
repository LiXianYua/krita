/*
 * SPDX-FileCopyrightText: 2023 Sharaf Zaman <shzam@sdf.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef __KISRESOURCETHUMBNAILCACHE_H_
#define __KISRESOURCETHUMBNAILCACHE_H_

#include <PkImage.h>
#include <PkScopedPointer.h>
#include <PkSize.h>
#include <PkString.h>

#include <utility>

#include "kritaresources_export.h"

class KRITARESOURCES_EXPORT KisResourceThumbnailCache
{
public:
    KisResourceThumbnailCache();
    ~KisResourceThumbnailCache();

    /**
     * Return the process-wide cache, creating it on first use. Once shutdown()
     * begins this returns nullptr forever.
     */
    static KisResourceThumbnailCache *instance();

    /**
     * Clear and destroy the cache during application teardown. The call is
     * idempotent and terminal; instance() cannot create another cache
     * generation after shutdown begins. All cache users must stop first. This
     * must run before locator shutdown because cache keys may consult an
     * already-existing locator.
     */
    static void shutdown();

    /**
     * Return a cached/scaled thumbnail. storageLocation may use the locator's
     * relative storage spelling. When an initialized locator already exists,
     * cache keys store the absolute spelling so relative and absolute calls
     * share one entry and invalidation path. This lookup never creates or
     * revives the locator; without an initialized locator the spelling is kept.
     */
    PkImage getImage(const PkString &storageLocation,
                     const PkString &resourceType,
                     const PkString &filename,
                     const PkImage &source = PkImage(),
                     const PkSize size = PkSize(),
                     Qt::AspectRatioMode aspectMode = Qt::IgnoreAspectRatio,
                     Qt::TransformationMode transformMode = Qt::FastTransformation);

private:
    friend class KisResourceQueryMapper;
    friend class KisResourceLocator;
    friend class KisStorageModel;

    /*
     * Check if we have the original image in the cache.
     */
    PkImage originalImage(const PkString &storageLocation, const PkString &resourceType, const PkString &filename) const;
    void insert(const PkString &storageLocation,
                const PkString &resourceType,
                const PkString &filename,
                const PkImage &image);
    void insert(const std::pair<PkString, PkString> &key, const PkImage &image);

    void remove(const PkString &storageLocation, const PkString &resourceType, const PkString &filename);
    void remove(const std::pair<PkString, PkString> &key);

private:
    struct Private;
    PkScopedPointer<Private> m_d;
};

#endif // __KISRESOURCETHUMBNAILCACHE_H_
