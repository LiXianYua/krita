/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KISMEMORYSTORAGE_H
#define KISMEMORYSTORAGE_H

#include <KisStoragePlugin.h>

#include <kritaresources_export.h>

/**
 * @brief The KisMemoryStorage class stores the temporary resources
 * that are not saved to disk or bundle. It is also used to stores
 * transient per-document resources, such as the document-local palette
 * list.
 */
class KRITARESOURCES_EXPORT KisMemoryStorage : public KisStoragePlugin
{
public:
    KisMemoryStorage(const PkString &location = PkString("memory"));
    virtual ~KisMemoryStorage();

    /// Copying the memory storage clones all contained resources and tags
    KisMemoryStorage(const KisMemoryStorage &rhs);

    /// This clones all contained resources and tags from rhs
    KisMemoryStorage &operator=(const KisMemoryStorage &rhs);

    bool saveAsNewVersion(const PkString &resourceType, KoResourceSP resource) override;

    KisResourceStorage::ResourceItem resourceItem(const PkString &url) override;
    bool loadVersionedResource(KoResourceSP resource) override;
    bool importResource(const PkString &url, PkStream *device) override;
    bool exportResource(const PkString &url, PkStream *device) override;
    bool addResource(const PkString &resourceType,  KoResourceSP resource) override;

    PkString resourceMd5(const PkString &url) override;
    PkSharedPointer<KisResourceStorage::ResourceIterator> resources(const PkString &resourceType) override;
    PkSharedPointer<KisResourceStorage::TagIterator> tags(const PkString &resourceType) override;

    void setMetaData(const PkString &key, const PkVariant &value) override;
    PkStringList metaDataKeys() const override;
    PkVariant metaData(const PkString &key) const override;

private:
    friend class TestMemoryStorage;
    friend class TestResourceLocator;
    friend class TestStorageWrapper;
    bool testingRemoveResource(const PkString &url);
    bool testingAddTag(const PkString &resourceType, KisTagSP tag);
    bool testingRemoveTag(const PkString &resourceType, const PkString &tagUrl);

private:
    class Private;
    PkScopedPointer<Private> d;

};


#endif // KISMEMORYSTORAGE_H
