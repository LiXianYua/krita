/*
 * SPDX-FileCopyrightText: 2025 Dmitry Kazakov <dimula73@gmail.com>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#ifndef KISRESOURCEMETADATAMODEL_H
#define KISRESOURCEMETADATAMODEL_H

#include <PkString.h>
#include <PkVariant.h>

#include "KisResourceCacheDb.h"
#include "kritaresources_export.h"

/** Fetches individual metadata values through the Locator data API. */
class KRITARESOURCES_EXPORT KisResourceMetaDataModel
{
public:
    struct MetaDataValueResult
    {
        enum class State {
            Missing,
            DecodedValue,
            TypedNull,
            InvalidNull,
            Opaque,
            QueryError,
            ResourceLimitExceeded
        };

        PkString key;
        State state = State::Missing;
        PkVariant value;
        KisResourceCacheDb::MetaDataDecodeStatus status =
            KisResourceCacheDb::MetaDataDecodeStatus::ReadCorruptData;
        PkString rawBase64;
        bool rawBase64Available = false;

        bool hasDecodedValue() const
        {
            return state == State::DecodedValue ||
                   state == State::TypedNull ||
                   state == State::InvalidNull;
        }
    };

    explicit KisResourceMetaDataModel(const PkString &tableName);
    ~KisResourceMetaDataModel();

    MetaDataValueResult metaDataValueResult(int resourceId,
                                            const PkString &key) const;
    PkVariant metaDataValue(int resourceId, const PkString &key) const;

private:
    struct Private;
    Private *const d;
};

#endif // KISRESOURCEMETADATAMODEL_H
