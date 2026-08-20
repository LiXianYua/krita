/*
 * SPDX-FileCopyrightText: 2025 Dmitry Kazakov <dimula73@gmail.com>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#ifndef KISRESOURCEMETADATAMODEL_H
#define KISRESOURCEMETADATAMODEL_H

#include <PkString.h>
#include <PkVariant.h>

#include "kritaresources_export.h"

/** Fetches individual metadata values through the Locator data API. */
class KRITARESOURCES_EXPORT KisResourceMetaDataModel
{
public:
    explicit KisResourceMetaDataModel(const PkString &tableName);
    ~KisResourceMetaDataModel();

    PkVariant metaDataValue(int resourceId, const PkString &key) const;

private:
    struct Private;
    Private *const d;
};

#endif // KISRESOURCEMETADATAMODEL_H
