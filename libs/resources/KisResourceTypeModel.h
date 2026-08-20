/*
 * SPDX-FileCopyrightText: 2018 boud <boud@valdyas.org>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KISRESOURCETYPEMODEL_H
#define KISRESOURCETYPEMODEL_H

#include <PkString.h>
#include <PkVector.h>

#include "kritaresources_export.h"

struct KRITARESOURCES_EXPORT KisResourceTypeRecord
{
    int id = -1;
    PkString resourceType;
    PkString displayName;
};

/** A snapshot of resource types registered in the cache database. */
class KRITARESOURCES_EXPORT KisResourceTypeModel
{
public:
    KisResourceTypeModel();
    ~KisResourceTypeModel();

    KisResourceTypeModel(const KisResourceTypeModel &) = delete;
    KisResourceTypeModel &operator=(const KisResourceTypeModel &) = delete;

    PkVector<KisResourceTypeRecord> resourceTypes() const;
    bool refresh();

private:
    struct Private;
    Private *const d;
};

#endif // KISRESOURCETYPEMODEL_H
