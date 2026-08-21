/*
 * SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <PkXmlCompat.h>

#include "KoColorConversionCache.h"

#include <PkHash.h>
#include <PkList.h>
#include <PkPair.h>
#include <PkAtomic.h>
#include <PkMutex.h>
#include <PkThreadStorage.h>

#include <KoColorSpace.h>

struct KoColorConversionCacheKey {

    KoColorConversionCacheKey(const KoColorSpace* _src,
                              const KoColorSpace* _dst,
                              KoColorConversionTransformation::Intent _renderingIntent,
                              KoColorConversionTransformation::ConversionFlags _conversionFlags)
        : src(_src)
        , dst(_dst)
        , renderingIntent(_renderingIntent)
        , conversionFlags(_conversionFlags)
    {
    }

    bool operator==(const KoColorConversionCacheKey& rhs) const {
        return (*src == *(rhs.src)) && (*dst == *(rhs.dst))
                && (renderingIntent == rhs.renderingIntent)
                && (conversionFlags == rhs.conversionFlags);
    }

    const KoColorSpace* src;
    const KoColorSpace* dst;
    KoColorConversionTransformation::Intent renderingIntent;
    KoColorConversionTransformation::ConversionFlags conversionFlags;
};

unsigned int qHash(const KoColorConversionCacheKey& key)
{
    return qHash(key.src) + qHash(key.dst) + qHash(key.renderingIntent) + qHash(key.conversionFlags);
}

struct KoColorConversionCache::CachedTransformation {

    CachedTransformation(KoColorConversionTransformation* _transfo)
        : transfo(_transfo), use(0)
    {}

    ~CachedTransformation() {
        delete transfo;
    }

    bool isNotInUse() {
        return !use;
    }

    KoColorConversionTransformation* transfo;
    PkAtomicInt use;
};

typedef PkPair<KoColorConversionCacheKey, KoCachedColorConversionTransformation> FastPathCacheItem;

struct KoColorConversionCache::Private {
    PkHash< KoColorConversionCacheKey, PkList<CachedTransformation*> > cache;
    PkMutex cacheMutex;

    PkThreadStorage<FastPathCacheItem> fastStorage;
};


KoColorConversionCache::KoColorConversionCache() : d(new Private)
{
}

KoColorConversionCache::~KoColorConversionCache()
{
    for (auto& cts : d->cache) {
        for (CachedTransformation* transfo : cts) {
            delete transfo;
        }
    }
    delete d;
}

KoCachedColorConversionTransformation KoColorConversionCache::cachedConverter(const KoColorSpace* src,
                                                                              const KoColorSpace* dst,
                                                                              KoColorConversionTransformation::Intent _renderingIntent,
                                                                              KoColorConversionTransformation::ConversionFlags _conversionFlags)
{
    KoColorConversionCacheKey key(src, dst, _renderingIntent, _conversionFlags);

    FastPathCacheItem *cacheItem =
        d->fastStorage.localData();

    if (cacheItem) {
        if (cacheItem->first == key) {
            return cacheItem->second;
        }
    }

    cacheItem = 0;

    PkMutexLocker lock(&d->cacheMutex);
    PkList< CachedTransformation* > cachedTransfos = d->cache.value(key);
    if (!cachedTransfos.isEmpty()) {
        CachedTransformation* ct = cachedTransfos.first();
        ct->transfo->setSrcColorSpace(src);
        ct->transfo->setDstColorSpace(dst);

        cacheItem = new FastPathCacheItem(key, KoCachedColorConversionTransformation(ct));
    }
    if (!cacheItem) {
        KoColorConversionTransformation* transfo = src->createColorConverter(dst, _renderingIntent, _conversionFlags);
        CachedTransformation* ct = new CachedTransformation(transfo);
        d->cache[key].append(ct);
        cacheItem = new FastPathCacheItem(key, KoCachedColorConversionTransformation(ct));
    }

    d->fastStorage.setLocalData(cacheItem);
    return cacheItem->second;
}

void KoColorConversionCache::colorSpaceIsDestroyed(const KoColorSpace* cs)
{
    d->fastStorage.setLocalData(0);

    PkMutexLocker lock(&d->cacheMutex);
    PkHash< KoColorConversionCacheKey, PkList<CachedTransformation*> >::iterator it = d->cache.begin();
    while (it != d->cache.end()) {
        if (it.key().src == cs || it.key().dst == cs) {
            for (CachedTransformation* ct : it.value()) {
                Q_ASSERT(ct->isNotInUse()); // That's terribly evil, if that assert fails, that means that someone is using a color transformation with a color space which is currently being deleted
                delete ct;
            }
            it = d->cache.erase(it);
        } else {
            ++it;
        }
    }
}

//--------- KoCachedColorConversionTransformation ----------//

KoCachedColorConversionTransformation::KoCachedColorConversionTransformation(KoColorConversionCache::CachedTransformation* transfo)
    : m_transfo(transfo)
{
    m_transfo = transfo;
    m_transfo->use.ref();
}

KoCachedColorConversionTransformation::KoCachedColorConversionTransformation(const KoCachedColorConversionTransformation& rhs)
    : m_transfo(rhs.m_transfo)
{
    m_transfo->use.ref();
}

KoCachedColorConversionTransformation::~KoCachedColorConversionTransformation()
{
    Q_ASSERT(m_transfo->use > 0);
    m_transfo->use.deref();
}

const KoColorConversionTransformation* KoCachedColorConversionTransformation::transformation() const
{
    return m_transfo->transfo;
}

