/*
 *  SPDX-FileCopyrightText: 2021 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KoResourceCacheStorage.h"

#include <PkMap.h>
#include <PkString.h>
#include <PkVariant.h>

#include "kis_assert.h"

struct KoResourceCacheStorage::Private
{
    PkMap<PkString, PkVariant> map;
};

KoResourceCacheStorage::KoResourceCacheStorage()
    : m_d(new Private)
{
}

KoResourceCacheStorage::~KoResourceCacheStorage()
{
}

PkVariant KoResourceCacheStorage::fetch(const PkString &key) const
{
    return m_d->map.value(key, PkVariant());
}

void KoResourceCacheStorage::put(const PkString &key, const PkVariant &value)
{
    /// This assert here is intentional! It catches cache key
    /// aliasing problems. See dox in KoResourceCacheInterface
    KIS_SAFE_ASSERT_RECOVER_NOOP(!m_d->map.contains(key));

    m_d->map.insert(key, value);
}
