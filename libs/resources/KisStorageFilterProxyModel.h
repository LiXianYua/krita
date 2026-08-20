/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KISSTORAGEFILTERPROXYMODEL_H
#define KISSTORAGEFILTERPROXYMODEL_H

#include <PkVariant.h>
#include <PkVector.h>

#include <KisResourceStorage.h>
#include <KisStorageModel.h>

#include "kritaresources_export.h"

/** Ordinary typed filtering over the shared storage snapshot. */
class KRITARESOURCES_EXPORT KisStorageFilterProxyModel
{
public:
    enum FilterType {
        ByFileName = 0,
        ByStorageType,
        ByActive
    };

    KisStorageFilterProxyModel();
    ~KisStorageFilterProxyModel();

    KisStorageFilterProxyModel(const KisStorageFilterProxyModel &) = delete;
    KisStorageFilterProxyModel &operator=(const KisStorageFilterProxyModel &) = delete;

    void setFilter(FilterType filterType, const PkVariant &filter);
    PkVector<KisStorageRecord> storages() const;
    KisResourceStorageSP storageForId(int storageId) const;

private:
    struct Private;
    Private *const d;
};

#endif // KISSTORAGEFILTERPROXYMODEL_H
