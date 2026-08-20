/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KISRESOURCEMODEL_H
#define KISRESOURCEMODEL_H

#include <PkImage.h>
#include <PkMap.h>
#include <PkObject.h>
#include <PkString.h>
#include <PkStringList.h>
#include <PkVariant.h>
#include <PkVector.h>

#include <kritaresources_export.h>

#include <KisTag.h>
#include <KoResource.h>

class PkStream;

/** A cache-database row for a resource. */
struct KRITARESOURCES_EXPORT KisResourceRecord
{
    int id = -1;
    int storageId = -1;
    PkString name;
    PkString filename;
    PkString tooltip;
    PkImage thumbnail;
    bool status = false;
    PkString location;
    PkString resourceType;
    PkStringList tags;
    PkString md5;
    bool dirty = false;
    PkMap<PkString, PkVariant> metaData;
    bool resourceActive = false;
    bool storageActive = false;
};

/** Shared resource data operations implemented by data models and filters. */
class KRITARESOURCES_EXPORT KisAbstractResourceModel
{
public:
    enum Columns {
        Id = 0,
        StorageId,
        Name,
        Filename,
        Tooltip,
        Thumbnail,
        Status,
        Location,
        ResourceType,
        Tags,
        MD5,
        LargeThumbnail,
        Dirty,
        MetaData,
        ResourceActive,
        StorageActive,
        BrokenStatus,
        BrokenStatusMessage,
    };

    virtual ~KisAbstractResourceModel() = default;

    virtual PkVector<KisResourceRecord> records() const = 0;
    virtual KoResourceSP resourceForId(int resourceId) const = 0;
    virtual bool setResourceActive(int resourceId, bool value) = 0;

    virtual bool setResourceActive(const KoResourceSP &resource, bool value)
    {
        return resource && resource->resourceId() >= 0 &&
            setResourceActive(resource->resourceId(), value);
    }

    bool setResourceInactive(int resourceId)
    {
        return setResourceActive(resourceId, false);
    }

    bool setResourceInactive(const KoResourceSP &resource)
    {
        return setResourceActive(resource, false);
    }

    virtual KoResourceSP importResourceFile(const PkString &filename,
                                             bool allowOverwrite,
                                             const PkString &storageId = PkString()) = 0;
    virtual KoResourceSP importResource(const PkString &filename,
                                         PkStream *device,
                                         bool allowOverwrite,
                                         const PkString &storageId = PkString()) = 0;
    virtual bool importWillOverwriteResource(const PkString &filename,
                                              const PkString &storageLocation = PkString()) const = 0;
    virtual bool exportResource(KoResourceSP resource, PkStream *device) = 0;
    virtual bool addResource(KoResourceSP resource,
                             const PkString &storageId = PkString()) = 0;
    virtual bool addResourceDeduplicateFileName(KoResourceSP resource,
                                                 const PkString &storageId) = 0;
    virtual bool updateResource(KoResourceSP resource) = 0;
    virtual bool reloadResource(KoResourceSP resource) = 0;
    virtual bool renameResource(KoResourceSP resource, const PkString &name) = 0;
    virtual bool setResourceMetaData(KoResourceSP resource,
                                     PkMap<PkString, PkVariant> metadata) = 0;
};

class KRITARESOURCES_EXPORT KisAbstractResourceFilterInterface
{
public:
    virtual ~KisAbstractResourceFilterInterface() = default;

    enum ResourceFilter {
        ShowInactiveResources = 0,
        ShowActiveResources,
        ShowAllResources
    };

    enum StorageFilter {
        ShowInactiveStorages = 0,
        ShowActiveStorages,
        ShowAllStorages
    };

    virtual void setResourceFilter(ResourceFilter filter) = 0;
    virtual void setStorageFilter(StorageFilter filter) = 0;
};

/** Shared cached snapshot of every database resource of one type. */
class KRITARESOURCES_EXPORT KisAllResourcesModel final
    : public PkObject
    , public KisAbstractResourceModel
{
private:
    friend class KisResourceModelProvider;
    friend class KisResourceModel;
    friend class KisResourceQueryMapper;

    explicit KisAllResourcesModel(const PkString &resourceType,
                                  PkObject *parent = nullptr);

public:
    ~KisAllResourcesModel() override;

    using KisAbstractResourceModel::setResourceActive;

    PkVector<KisResourceRecord> records() const override;
    PkVector<KoResourceSP> resources() const;
    KoResourceSP resourceForId(int id) const override;
    bool resourceExists(const PkString &md5,
                        const PkString &filename,
                        const PkString &name) const;
    PkVector<KoResourceSP> resourcesForFilename(const PkString &filename) const;
    PkVector<KoResourceSP> resourcesForName(const PkString &name) const;
    PkVector<KoResourceSP> resourcesForMD5(const PkString &md5sum) const;
    PkVector<KisTagSP> tagsForResource(int resourceId) const;

    bool setResourceActive(int resourceId, bool value) override;
    KoResourceSP importResourceFile(const PkString &filename,
                                     bool allowOverwrite,
                                     const PkString &storageId = PkString()) override;
    KoResourceSP importResource(const PkString &filename,
                                 PkStream *device,
                                 bool allowOverwrite,
                                 const PkString &storageId = PkString()) override;
    bool importWillOverwriteResource(const PkString &filename,
                                      const PkString &storageLocation = PkString()) const override;
    bool exportResource(KoResourceSP resource, PkStream *device) override;
    bool addResource(KoResourceSP resource,
                     const PkString &storageId = PkString()) override;
    bool addResourceDeduplicateFileName(KoResourceSP resource,
                                         const PkString &storageId = PkString()) override;
    bool updateResource(KoResourceSP resource) override;
    bool reloadResource(KoResourceSP resource) override;
    bool renameResource(KoResourceSP resource, const PkString &name) override;
    bool setResourceMetaData(KoResourceSP resource,
                             PkMap<PkString, PkVariant> metadata) override;

private:
    void storageActiveStateChanged(const PkString &location);
    void storageResynchronized(const PkString &storage, bool bulk);
    void storagesBulkSynchronizationFinished();
    void beginExternalResourceImport(const PkString &resourceType, int count);
    void endExternalResourceImport(const PkString &resourceType);
    void beginExternalResourceRemove(const PkString &resourceType,
                                     PkVector<int> resourceIds);
    void endExternalResourceRemove(const PkString &resourceType);
    void slotResourceActiveStateChanged(const PkString &resourceType,
                                        int resourceId);

    bool refresh();
    void closeQuery();

    struct Private;
    Private *const d;
};

/** Ordinary filtering view over the shared all-resource snapshot. */
class KRITARESOURCES_EXPORT KisResourceModel
    : public KisAbstractResourceModel
    , public KisAbstractResourceFilterInterface
{
public:
    explicit KisResourceModel(const PkString &type);
    ~KisResourceModel() override;

    KisResourceModel(const KisResourceModel &) = delete;
    KisResourceModel &operator=(const KisResourceModel &) = delete;

    using KisAbstractResourceModel::setResourceActive;

    void setResourceFilter(ResourceFilter filter) override;
    void setStorageFilter(StorageFilter filter) override;
    void showOnlyUntaggedResources(bool showOnlyUntagged);

    PkVector<KisResourceRecord> records() const override;
    PkVector<KoResourceSP> resources() const;
    KoResourceSP resourceForId(int id) const override;
    bool setResourceActive(int resourceId, bool value) override;
    KoResourceSP importResourceFile(const PkString &filename,
                                     bool allowOverwrite,
                                     const PkString &storageId = PkString()) override;
    KoResourceSP importResource(const PkString &filename,
                                 PkStream *device,
                                 bool allowOverwrite,
                                 const PkString &storageId = PkString()) override;
    bool importWillOverwriteResource(const PkString &filename,
                                      const PkString &storageLocation = PkString()) const override;
    bool exportResource(KoResourceSP resource, PkStream *device) override;
    bool addResource(KoResourceSP resource,
                     const PkString &storageId = PkString()) override;
    bool addResourceDeduplicateFileName(KoResourceSP resource,
                                         const PkString &storageId) override;
    bool updateResource(KoResourceSP resource) override;
    bool reloadResource(KoResourceSP resource) override;
    bool renameResource(KoResourceSP resource, const PkString &name) override;
    bool setResourceMetaData(KoResourceSP resource,
                             PkMap<PkString, PkVariant> metadata) override;

    PkVector<KoResourceSP> resourcesForFilename(const PkString &filename) const;
    PkVector<KoResourceSP> resourcesForName(const PkString &name) const;
    PkVector<KoResourceSP> resourcesForMD5(const PkString &md5sum) const;
    PkVector<KisTagSP> tagsForResource(int resourceId) const;

private:
    bool accepts(const KisResourceRecord &record) const;
    PkVector<KoResourceSP> resourcesMatching(const PkString &value,
                                             Columns column) const;

    struct Private;
    Private *const d;
};

#endif // KISRESOURCEMODEL_H
