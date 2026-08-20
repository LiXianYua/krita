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
    if (d->tableName != PkString("resources") || resourceId < 0) {
        return PkVariant();
    }
    KisResourceLocator *locator = KisResourceLocator::instance();
    return locator ? locator->metaDataForResource(resourceId).value(key)
                   : PkVariant();
}
