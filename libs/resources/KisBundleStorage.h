/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KISBUNDLESTORAGE_H
#define KISBUNDLESTORAGE_H

#include <KisStoragePlugin.h>
#include "kritaresources_export.h"

/**
 * KisBundleStorage is KisStoragePlugin that can load resources 
 * from bundles. It can also manage overridden resources from bundles,
 * which are not stored in the bundles themselves.
 */
class KRITARESOURCES_EXPORT KisBundleStorage : public KisStoragePlugin
{
public:
    KisBundleStorage(const PkString &location);
    virtual ~KisBundleStorage();

    KisResourceStorage::ResourceItem resourceItem(const PkString &url) override;

    /// Note: this should find resources in a folder that override a resource in the bundle first
    bool loadVersionedResource(KoResourceSP resource) override;
    PkString resourceMd5(const PkString &url) override;
    PkSharedPointer<KisResourceStorage::ResourceIterator> resources(const PkString &resourceType) override;
    PkSharedPointer<KisResourceStorage::TagIterator> tags(const PkString &resourceType) override;
    PkImage thumbnail() const override;
    PkStringList metaDataKeys() const override;
    PkVariant metaData(const PkString &key) const override;

    /// Add a resource to this bundle: note, the bundle itself should NOT be rewritten, but we need to
    /// put these tags in a place in the file system
    bool saveAsNewVersion(const PkString &resourceType, KoResourceSP resource) override;

    bool exportResource(const PkString &url, PkStream *device) override;

private:
    friend class BundleIterator;

private:
    class Private;
    PkScopedPointer<Private> d;
};

#endif // KISBUNDLESTORAGE_H
