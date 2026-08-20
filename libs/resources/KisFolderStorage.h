/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KISFOLDERSTORAGE_H
#define KISFOLDERSTORAGE_H

#include <KisStoragePlugin.h>

#include <kritaresources_export.h>

/**
 * KisFolderStorage is a KisStoragePlugin which handles resources 
 * stored in the user's resource folder. On initial startup, every
 * resource that comes as a folder resource is copied to the user's
 * resource folder. This is also the default location where the
 * resources the user creates are stored. 
 */
class KRITARESOURCES_EXPORT KisFolderStorage : public KisStoragePlugin
{
public:
    KisFolderStorage(const PkString &location);
    virtual ~KisFolderStorage();

    /// Adds or updates this resource to the storage
    bool saveAsNewVersion(const PkString &resourceType, KoResourceSP resource) override;

    KisResourceStorage::ResourceItem resourceItem(const PkString &url) override;
    bool loadVersionedResource(KoResourceSP resource) override;
    PkSharedPointer<KisResourceStorage::ResourceIterator> resources(const PkString &resourceType) override;
    PkSharedPointer<KisResourceStorage::TagIterator> tags(const PkString &resourceType) override;
    bool importResource(const PkString &url, PkStream *device) override;
    bool exportResource(const PkString &url, PkStream *device) override;
    bool addResource(const PkString  &resourceType, KoResourceSP resource) override;

    PkStringList metaDataKeys() const override;
    PkVariant metaData(const PkString &key) const override;

    PkString resourceMd5(const PkString &url) override;
    PkString resourceFilePath(const PkString &url) override;
private:
    friend class FolderIterator;

};

#endif // KISFOLDERSTORAGE_H
