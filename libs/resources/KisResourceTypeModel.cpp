/*
 * SPDX-FileCopyrightText: 2018 boud <boud@valdyas.org>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisResourceTypeModel.h"

#include <PkSqlQuery.h>

#include "KisResourceTypes.h"

namespace
{

PkString displayNameForType(const PkString &type)
{
    if (type == ResourceType::PaintOpPresets) return ResourceName::PaintOpPresets;
    if (type == ResourceType::Brushes) return ResourceName::Brushes;
    if (type == ResourceType::Gradients) return ResourceName::Gradients;
    if (type == ResourceType::Palettes) return ResourceName::Palettes;
    if (type == ResourceType::Patterns) return ResourceName::Patterns;
    if (type == ResourceType::Workspaces) return ResourceName::Workspaces;
    if (type == ResourceType::Symbols) return ResourceName::Symbols;
    if (type == ResourceType::WindowLayouts) return ResourceName::WindowLayouts;
    if (type == ResourceType::Sessions) return ResourceName::Sessions;
    if (type == ResourceType::GamutMasks) return ResourceName::GamutMasks;
    if (type == ResourceType::SeExprScripts) return ResourceName::SeExprScripts;
    if (type == ResourceType::TaskSets) return ResourceName::TaskSets;
    if (type == ResourceType::LayerStyles) return ResourceName::LayerStyles;
    if (type == ResourceType::FontFamilies) return ResourceName::FontFamilies;
    if (type == ResourceType::CssStyles) return ResourceName::CssStyles;
    return type;
}

} // namespace

struct KisResourceTypeModel::Private
{
    PkVector<KisResourceTypeRecord> records;
};

KisResourceTypeModel::KisResourceTypeModel()
    : d(new Private)
{
    refresh();
}

KisResourceTypeModel::~KisResourceTypeModel()
{
    delete d;
}

PkVector<KisResourceTypeRecord> KisResourceTypeModel::resourceTypes() const
{
    return d->records;
}

bool KisResourceTypeModel::refresh()
{
    PkSqlQuery query;
    if (!query.exec(PkString("SELECT id, name FROM resource_types ORDER BY id"))) {
        return false;
    }

    PkVector<KisResourceTypeRecord> replacement;
    while (query.next()) {
        KisResourceTypeRecord record;
        record.id = query.value(PkString("id")).toInt();
        record.resourceType = query.value(PkString("name")).toString();
        record.displayName = displayNameForType(record.resourceType);
        replacement.append(record);
    }
    d->records = replacement;
    return true;
}
