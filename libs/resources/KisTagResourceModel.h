/*
 * SPDX-FileCopyrightText: 2020 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#ifndef KISTAGRESOURCEMODEL_H
#define KISTAGRESOURCEMODEL_H

#include <PkObject.h>
#include <PkVector.h>

#include <KisResourceModel.h>
#include <KisTag.h>
#include <KoResource.h>

#include "kritaresources_export.h"

struct KRITARESOURCES_EXPORT KisTagResourceRecord
{
    int tagId = -1;
    int resourceId = -1;
    KisTagSP tag;
    KisResourceRecord resource;
    bool tagActive = false;
    bool resourceActive = false;
    bool resourceStorageActive = false;
    PkString tagName;
};

class KRITARESOURCES_EXPORT KisAbstractTagResourceModel
{
public:
    virtual ~KisAbstractTagResourceModel() = default;

    virtual bool tagResources(const KisTagSP &tag,
                              const PkVector<int> &resourceIds) = 0;
    virtual bool untagResources(const KisTagSP &tag,
                                const PkVector<int> &resourceIds) = 0;
    virtual int isResourceTagged(const KisTagSP &tag, int resourceId) = 0;
};

/** Shared cached snapshot of active tag-resource relations. */
class KRITARESOURCES_EXPORT KisAllTagResourceModel final
    : public PkObject
    , public KisAbstractTagResourceModel
{
public:
    ~KisAllTagResourceModel() override;

    PkVector<KisTagResourceRecord> relations() const;
    bool tagResources(const KisTagSP &tag,
                      const PkVector<int> &resourceIds) override;
    bool untagResources(const KisTagSP &tag,
                        const PkVector<int> &resourceIds) override;
    int isResourceTagged(const KisTagSP &tag, int resourceId) override;

private:
    friend class KisResourceModelProvider;
    friend class KisTagResourceModel;

    explicit KisAllTagResourceModel(const PkString &resourceType,
                                    PkObject *parent = nullptr);

    void storageChanged(const PkString &location);
    void slotResourceActiveStateChanged(const PkString &resourceType,
                                        int resourceId);
    bool refresh();
    void closeQuery();

    struct Private;
    Private *const d;
};

/** Ordinary filters over tag-resource relations plus resource operations. */
class KRITARESOURCES_EXPORT KisTagResourceModel
    : public KisAbstractTagResourceModel
    , public KisAbstractResourceModel
    , public KisAbstractResourceFilterInterface
{
public:
    enum TagFilter {
        ShowInactiveTags = 0,
        ShowActiveTags,
        ShowAllTags
    };

    explicit KisTagResourceModel(const PkString &resourceType);
    ~KisTagResourceModel() override;

    KisTagResourceModel(const KisTagResourceModel &) = delete;
    KisTagResourceModel &operator=(const KisTagResourceModel &) = delete;

    using KisAbstractResourceModel::setResourceActive;

    void setTagFilter(TagFilter filter);
    void setResourceFilter(ResourceFilter filter) override;
    void setStorageFilter(StorageFilter filter) override;

    void setTagsFilter(const PkVector<int> &tagIds);
    void setResourcesFilter(const PkVector<int> &resourceIds);
    void setTagsFilter(const PkVector<KisTagSP> &tags);
    void setResourcesFilter(const PkVector<KoResourceSP> &resources);

    PkVector<KisTagResourceRecord> relations() const;
    PkVector<KisResourceRecord> records() const override;
    PkVector<KoResourceSP> resources() const;

    bool tagResources(const KisTagSP &tag,
                      const PkVector<int> &resourceIds) override;
    bool untagResources(const KisTagSP &tag,
                        const PkVector<int> &resourceIds) override;
    int isResourceTagged(const KisTagSP &tag, int resourceId) override;

    KoResourceSP resourceForId(int resourceId) const override;
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

private:
    bool accepts(const KisTagResourceRecord &record) const;

    struct Private;
    Private *const d;
};

#endif // KISTAGRESOURCEMODEL_H
