/*
 * SPDX-FileCopyrightText: 2023 Sharaf Zaman <shzam@sdf.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KisResourceThumbnailCache.h"

#include <PkMap.h>

#include <kis_assert.h>

struct ImageScalingParameters {
    PkSize size;
    Qt::AspectRatioMode aspectRatioMode;
    Qt::TransformationMode transformationMode;

    bool operator<(const ImageScalingParameters &other) const
    {
        if (size != other.size) {
            if (size.width() != other.size.width()) {
                return size.width() < other.size.width();
            } else {
                return size.height() < other.size.height();
            }
        } else if (aspectRatioMode != other.aspectRatioMode) {
            return aspectRatioMode < other.aspectRatioMode;
        } else if (transformationMode != other.transformationMode) {
            return transformationMode < other.transformationMode;
        } else {
            // they're the same
            return false;
        }
    }
};

namespace
{
using ResourceKey = std::pair<PkString, PkString>;
using ThumbnailCacheT = PkMap<ImageScalingParameters, PkImage>;
} // namespace

struct KisResourceThumbnailCache::Private {
    PkMap<ResourceKey, ThumbnailCacheT> scaledThumbnailCache;
    PkMap<ResourceKey, PkImage> originalImageCache;

    PkImage getExactMatch(const ResourceKey &key, ImageScalingParameters param) const;
    PkImage getOriginal(const ResourceKey &key) const;
    void insertOriginal(const ResourceKey &key, const PkImage &image);
    bool containsOriginal(const ResourceKey &key) const;

    ResourceKey
    key(const PkString &storageLocation, const PkString &resourceType, const PkString &filename) const;
};

PkImage KisResourceThumbnailCache::Private::getExactMatch(const ResourceKey &key,
                                                         ImageScalingParameters param) const
{
    const auto thumbnailEntries = scaledThumbnailCache.find(key);
    if (thumbnailEntries != scaledThumbnailCache.end()) {
        const auto scaledThumbnail = thumbnailEntries->find(param);
        if (scaledThumbnail != thumbnailEntries->end()) {
            return *scaledThumbnail;
        }
    }

    const auto originalImage = originalImageCache.find(key);
    if (originalImage != originalImageCache.end() && originalImage->size() == param.size) {
        return *originalImage;
    }

    return PkImage();
}

PkImage KisResourceThumbnailCache::Private::getOriginal(const ResourceKey &key) const
{
    return originalImageCache[key];
}

void KisResourceThumbnailCache::Private::insertOriginal(const ResourceKey &key, const PkImage &image)
{
    // Someone else has added the image to this cache, when the only path to here is from a method which
    // checks whether this cache contains it or not.
    KIS_ASSERT(!originalImageCache.contains(key));
    originalImageCache.insert(key, image);
}

bool KisResourceThumbnailCache::Private::containsOriginal(const ResourceKey &key) const
{
    return originalImageCache.contains(key);
}

ResourceKey KisResourceThumbnailCache::Private::key(const PkString &storageLocation,
                                                    const PkString &resourceType,
                                                    const PkString &filename) const
{
    return {storageLocation, resourceType + "/" + filename};
}

KisResourceThumbnailCache *KisResourceThumbnailCache::instance()
{
    static KisResourceThumbnailCache cache;
    return &cache;
}

KisResourceThumbnailCache::KisResourceThumbnailCache()
    : m_d(new Private)
{
}

KisResourceThumbnailCache::~KisResourceThumbnailCache()
{
}

PkImage KisResourceThumbnailCache::originalImage(const PkString &storageLocation,
                                                 const PkString &resourceType,
                                                 const PkString &filename) const
{
    const ResourceKey key = m_d->key(storageLocation, resourceType, filename);
    return m_d->containsOriginal(key) ? m_d->getOriginal(key) : PkImage();
}

void KisResourceThumbnailCache::insert(const PkString &storageLocation,
                                       const PkString &resourceType,
                                       const PkString &filename,
                                       const PkImage &image)
{
    if (image.isNull()) {
        return;
    }
    insert(m_d->key(storageLocation, resourceType, filename), image);
}

void KisResourceThumbnailCache::insert(const std::pair<PkString, PkString> &key, const PkImage &image)
{
    m_d->insertOriginal(key, image);
}

void KisResourceThumbnailCache::remove(const PkString &storageLocation,
                                       const PkString &resourceType,
                                       const PkString &filename)
{
    remove(m_d->key(storageLocation, resourceType, filename));
}

void KisResourceThumbnailCache::remove(const std::pair<PkString, PkString> &key)
{
    if (m_d->originalImageCache.contains(key)) {
        m_d->originalImageCache.remove(key);

        if (m_d->scaledThumbnailCache.contains(key)) {
            m_d->scaledThumbnailCache.remove(key);
        }
    } else {
        // Something must have gone wrong for thumbnail to exist in scaledThumbnailCache but not be in
        // original.
        KIS_ASSERT(!m_d->scaledThumbnailCache.contains(key));
    }
}

PkImage KisResourceThumbnailCache::getImage(const PkString &storageLocation,
                                            const PkString &resourceType,
                                            const PkString &filename,
                                            const PkImage &source,
                                            const PkSize size,
                                           Qt::AspectRatioMode aspectMode,
                                           Qt::TransformationMode transformMode)
{
    const ImageScalingParameters param = {size, aspectMode, transformMode};

    ResourceKey key = m_d->key(storageLocation, resourceType, filename);

    PkImage result = m_d->getExactMatch(key, param);
    if (!result.isNull()) {
        return result;
    } else if (m_d->containsOriginal(key)) {
        result = m_d->getOriginal(key);
    } else {
        result = source;
        if (!result.isNull()) {
            m_d->insertOriginal(key, result);
        }
    }
    // if the size that the has been demanded, we will then cache the size and then pass it.
    if (!result.isNull() && param.size.isValid()) {
        const PkImage scaledImage = result.scaled(param.size, param.aspectRatioMode, param.transformationMode);
        if (m_d->scaledThumbnailCache.contains(key)) {
            m_d->scaledThumbnailCache[key].insert(param, scaledImage);
        } else {
            ThumbnailCacheT scaledCacheMap;
            scaledCacheMap.insert(param, scaledImage);
            m_d->scaledThumbnailCache.insert(key, scaledCacheMap);
        }
        return scaledImage;
    } else {
        return result;
    }
}
