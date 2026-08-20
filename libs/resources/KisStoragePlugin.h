/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KISSTORAGEPLUGIN_H
#define KISSTORAGEPLUGIN_H

#include <PkScopedPointer.h>
#include <PkString.h>

#include <KisResourceStorage.h>
#include "kritaresources_export.h"

class PkStream;

/**
 * The KisStoragePlugin class is the base class
 * for storage plugins. A storage plugin is used by
 * KisResourceStorage to locate resources and tags in
 * a kind of storage, like a folder, a bundle or an adobe
 * resource library.
 */
class KRITARESOURCES_EXPORT KisStoragePlugin
{
public:
    KisStoragePlugin(const PkString &location);
    virtual ~KisStoragePlugin();

    virtual KisResourceStorage::ResourceItem resourceItem(const PkString &url) = 0;

    /// Retrieve the given resource. The url is the unique identifier of the resource,
    /// for instance resourcetype plus filename.
    virtual KoResourceSP resource(const PkString &url);
    virtual PkString resourceMd5(const PkString &url);
    virtual PkString resourceFilePath(const PkString &url);
    virtual bool loadVersionedResource(KoResourceSP resource) = 0;
    virtual bool supportsVersioning() const;
    virtual PkSharedPointer<KisResourceStorage::ResourceIterator> resources(const PkString &resourceType) = 0;
    virtual PkSharedPointer<KisResourceStorage::TagIterator> tags(const PkString &resourceType) = 0;

    virtual bool saveAsNewVersion(const PkString &, KoResourceSP) { return false; }
    virtual bool importResource(const PkString &, PkStream *) { return false; }
    virtual bool exportResource(const PkString &, PkStream *) { return false; }
    virtual bool addResource(const PkString &, KoResourceSP) { return false; }
    virtual PkImage thumbnail() const { return PkImage(); }

    virtual void setMetaData(const PkString &, const PkVariant &) {}
    virtual PkStringList metaDataKeys() const { return PkStringList(); }
    virtual PkVariant metaData(const PkString &) const { return PkVariant(); }

    PkDateTime timestamp();

    virtual bool isValid() const;

protected:
    friend class TestBundleStorage;
    PkString location() const;

    /**
     * On some systems, e.g. Windows, the file names are case-insensitive,
     * therefore URLs will fetch the resource even when the casing is not
     * the same. The storage, when returning such a resource should make
     * sure that its filename is set to the **real** filename, not the one
     * with incorrect casing.
     */
    void sanitizeResourceFileNameCase(KoResourceSP resource, const PkString &parentDir);

private:
    class Private;
    PkScopedPointer<Private> d;
};

#endif // KISSTORAGEPLUGIN_H
