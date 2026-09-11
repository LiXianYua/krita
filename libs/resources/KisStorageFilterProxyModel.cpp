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

    for (const KisStorageRecord &record : d->source->storages()) {
        if (accepts(record)) {
            result.append(record);
        }
    }
    return result;
}

bool KisStorageFilterProxyModel::accepts(const KisStorageRecord &record) const
{
    if (!d->filter.isValid() || d->filter.isNull()) {
        return true;
    }

    switch (d->filterType) {
    case ByFileName:
        return record.location.contains(d->filter.toString());
    case ByStorageType:
        return d->filter.toStringList().contains(record.storageType);
    case ByActive:
        return record.active == d->filter.toBool();
    }
    return false;
}

KisResourceStorageSP KisStorageFilterProxyModel::storageForId(int storageId) const
{
    if (!d->source) {
        return KisResourceStorageSP();
    }

    // Fetch just this one record: storages() copies the whole table and reads the
    // metadata of every record, which is the O(N) cost this path must not pay.
    const KisStorageRecord record = d->source->recordForId(storageId);
    if (record.id != storageId || !accepts(record)) {
        return KisResourceStorageSP();
    }
    return d->source->storageForId(storageId);
}
