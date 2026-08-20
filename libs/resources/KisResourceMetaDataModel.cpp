/*
 * SPDX-FileCopyrightText: 2025 Dmitry Kazakov <dimula73@gmail.com>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KisResourceMetaDataModel.h"

#include "KisResourceLocator.h"

struct KisResourceMetaDataModel::Private
{
    PkString tableName;
};

KisResourceMetaDataModel::KisResourceMetaDataModel(const PkString &tableName)
    : d(new Private)
{
    d->tableName = tableName;
}

KisResourceMetaDataModel::~KisResourceMetaDataModel()
{
    delete d;
}

PkVariant KisResourceMetaDataModel::metaDataValue(
    int resourceId,
    const PkString &key) const
{
    return metaDataValueResult(resourceId, key).value;
}

KisResourceMetaDataModel::MetaDataValueResult
KisResourceMetaDataModel::metaDataValueResult(
    int resourceId,
    const PkString &key) const
{
    MetaDataValueResult result;
    result.key = key;

    if (d->tableName != PkString("resources") || resourceId < 0) {
        result.state = MetaDataValueResult::State::QueryError;
        return result;
    }

    KisResourceLocator *locator = KisResourceLocator::instance();
    if (!locator) {
        result.state = MetaDataValueResult::State::QueryError;
        return result;
    }

    const KisResourceCacheDb::MetaDataReadResult readResult =
        locator->metaDataReadResultForResource(resourceId);
    if (!readResult.querySucceeded) {
        result.state = MetaDataValueResult::State::QueryError;
        return result;
    }

    for (const KisResourceCacheDb::MetaDataReadRow &row : readResult.rows) {
        if (!row.keyAvailable || row.key != key) {
            continue;
        }

        result.rowId = row.rowId;
        result.keyStorageClass = row.keyStorageClass;
        result.valueStorageClass = row.valueStorageClass;
        result.rawKey = row.rawKey;
        result.rawKeyAvailable = row.rawKeyAvailable;
        result.rawPayload = row.rawPayload;
        result.rawPayloadBytesAvailable = row.rawPayloadAvailable;
        if (row.decoded) {
            result.value = row.value;
            if (!result.value.isValid()) {
                result.state = MetaDataValueResult::State::InvalidNull;
            } else if (result.value.isNull()) {
                result.state = MetaDataValueResult::State::TypedNull;
            } else {
                result.state = MetaDataValueResult::State::DecodedValue;
            }
        } else {
            result.state = MetaDataValueResult::State::Opaque;
            result.status = row.status;
            if (readResult.undecodable.contains(key)) {
                const KisResourceCacheDb::MetaDataDecodeIssue issue =
                    readResult.undecodable.value(key);
                result.rawBase64 = issue.rawPayload;
                result.rawBase64Available = issue.rawPayloadAvailable;
            }
        }
        return result;
    }

    result.state = readResult.resourceLimitExceeded
        ? MetaDataValueResult::State::ResourceLimitExceeded
        : MetaDataValueResult::State::Missing;
    return result;
}
