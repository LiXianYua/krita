/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-FileCopyrightText: 2019 Agata Cacko <cacko.azh@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KISABRSTORAGE_H
#define KISABRSTORAGE_H

#include <KisStoragePlugin.h>

#include <kritabrush_export.h>
#include <kis_abr_brush_collection.h>

class BRUSH_EXPORT KisAbrStorage : public KisStoragePlugin
{
public:
    KisAbrStorage(const PkString &location);
    virtual ~KisAbrStorage();

    KisResourceStorage::ResourceItem resourceItem(const PkString &url) override;

    KoResourceSP resource(const PkString &url) override;
    bool loadVersionedResource(KoResourceSP resource) override;
    bool supportsVersioning() const override;
    PkSharedPointer<KisResourceStorage::ResourceIterator> resources(const PkString &resourceType) override;
    PkSharedPointer<KisResourceStorage::TagIterator> tags(const PkString &resourceType) override;
    PkImage thumbnail() const override;
    KisAbrBrushCollectionSP m_brushCollection;
};

#endif // KISABRSTORAGE_H
