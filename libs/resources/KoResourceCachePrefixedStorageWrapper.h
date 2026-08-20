/*
 *  SPDX-FileCopyrightText: 2021 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KORESOURCECACHEPREFIXEDSTORAGEWRAPPER_H
#define KORESOURCECACHEPREFIXEDSTORAGEWRAPPER_H

#include <KoResourceCacheInterface.h>
#include <PkString.h>

/**
 * A simple wrapper class that converts all the passed cache keys
 * into a prefixed notation: "key" -> "prefix/key".
 *
 * This wrapper is usually needed when handing embedded objects
 * using the same cache storage, e.g. masking brush preset, which
 * is stored inside a normal preset under a prefix
 * ("MaskingBrush/Preset/")
 */
class KRITARESOURCES_EXPORT KoResourceCachePrefixedStorageWrapper : public KoResourceCacheInterface
{
public:
    KoResourceCachePrefixedStorageWrapper(const PkString &prefix, KoResourceCacheInterfaceSP baseInterface);

    PkVariant fetch(const PkString &key) const override;
    void put(const PkString &key, const PkVariant &value) override;

private:
    PkString m_prefix;
    KoResourceCacheInterfaceSP m_baseInterface;

};

#endif // KORESOURCECACHEPREFIXEDSTORAGEWRAPPER_H
