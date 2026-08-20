/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KisStorageFilterProxyModel.h"

struct KisStorageFilterProxyModel::Private
{
    KisStorageModel *source = nullptr;
    FilterType filterType = ByStorageType;
    PkVariant filter;
};

KisStorageFilterProxyModel::KisStorageFilterProxyModel()
    : d(new Private)
{
    d->source = KisStorageModel::instance();
}

KisStorageFilterProxyModel::~KisStorageFilterProxyModel()
{
    delete d;
}

void KisStorageFilterProxyModel::setFilter(FilterType filterType,
                                           const PkVariant &filter)
{
    d->filterType = filterType;
    d->filter = filter;
}

PkVector<KisStorageRecord> KisStorageFilterProxyModel::storages() const
{
    PkVector<KisStorageRecord> result;
    if (!d->source) {
        return result;
    }
    if (!d->filter.isValid() || d->filter.isNull()) {
        return d->source->storages();
    }

    for (const KisStorageRecord &record : d->source->storages()) {
        bool accepted = false;
        switch (d->filterType) {
        case ByFileName:
            accepted = record.location.contains(d->filter.toString());
            break;
        case ByStorageType:
            accepted = d->filter.toStringList().contains(record.storageType);
            break;
        case ByActive:
            accepted = record.active == d->filter.toBool();
            break;
        }
        if (accepted) {
            result.append(record);
        }
    }
    return result;
}

KisResourceStorageSP KisStorageFilterProxyModel::storageForId(int storageId) const
{
    for (const KisStorageRecord &record : storages()) {
        if (record.id == storageId) {
            return d->source->storageForId(storageId);
        }
    }
    return KisResourceStorageSP();
}
