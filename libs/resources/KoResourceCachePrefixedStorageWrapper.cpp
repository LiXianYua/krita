/*
 *  SPDX-FileCopyrightText: 2021 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KoResourceCachePrefixedStorageWrapper.h"

#include <PkVariant.h>


KoResourceCachePrefixedStorageWrapper::KoResourceCachePrefixedStorageWrapper(const PkString &prefix, KoResourceCacheInterfaceSP baseInterface)
    : m_prefix(prefix),
      m_baseInterface(baseInterface)
{
}

PkVariant KoResourceCachePrefixedStorageWrapper::fetch(const PkString &key) const
{
    return m_baseInterface->fetch(m_prefix + key);
}

void KoResourceCachePrefixedStorageWrapper::put(const PkString &key, const PkVariant &value)
{
    m_baseInterface->put(m_prefix + key, value);
}
