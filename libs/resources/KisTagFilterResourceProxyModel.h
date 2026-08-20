/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KISTAGFILTERRESOURCEPROXYMODEL_H
#define KISTAGFILTERRESOURCEPROXYMODEL_H

#include <KisResourceModel.h>
#include <KisTag.h>
#include <KisTagResourceModel.h>

#include "kritaresources_export.h"

class KisResourceSearchBoxFilter;

/** Combined tag, search, storage and metadata filtering for resources. */
class KRITARESOURCES_EXPORT KisTagFilterResourceProxyModel
    : public KisAbstractResourceModel
    , public KisAbstractResourceFilterInterface
{
public:
    explicit KisTagFilterResourceProxyModel(const PkString &resourceType);
    virtual ~KisTagFilterResourceProxyModel();

    KisTagFilterResourceProxyModel(const KisTagFilterResourceProxyModel &) = delete;
    KisTagFilterResourceProxyModel &operator=(const KisTagFilterResourceProxyModel &) = delete;

    using KisAbstractResourceModel::setResourceActive;

    void setResourceFilter(ResourceFilter filter) override;
    void setStorageFilter(StorageFilter filter) override;
    void setResourceModel(KisResourceModel *resourceModel);

    PkVector<KisResourceRecord> records() const override;
    PkVector<KoResourceSP> resources() const;
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
                                         const PkString &storageId = PkString()) override;
    bool updateResource(KoResourceSP resource) override;
    bool reloadResource(KoResourceSP resource) override;
    bool renameResource(KoResourceSP resource, const PkString &name) override;
    bool setResourceMetaData(KoResourceSP resource,
                             PkMap<PkString, PkVariant> metadata) override;

    virtual bool additionalResourceNameChecks(
        const KisResourceRecord &record,
        const KisResourceSearchBoxFilter *filter) const;

    void setMetaDataFilter(const PkMap<PkString, PkVariant> &metaDataMap);
    void setTagFilter(const KisTagSP &tag);
    KisTagSP currentTagFilter() const;
    void setStorageFilter(bool useFilter, int storageId);
    void setResourceFilter(const KoResourceSP &resource);
    void setSearchText(const PkString &searchText);
    void setFilterInCurrentTag(bool filterInCurrentTag);
    bool filterInCurrentTag() const;

    bool tagResources(const KisTagSP &tag, const PkVector<int> &resourceIds);
    bool untagResources(const KisTagSP &tag, const PkVector<int> &resourceIds);
    int isResourceTagged(const KisTagSP &tag, int resourceId);

private:
    PkVector<KisResourceRecord> sourceRecords() const;
    bool accepts(const KisResourceRecord &record) const;

    struct Private;
    Private *const d;
};

#endif // KISTAGFILTERRESOURCEPROXYMODEL_H
