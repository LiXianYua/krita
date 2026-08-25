/*
 *  SPDX-FileCopyrightText: 2024 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KOFONTSTORAGE_H
#define KOFONTSTORAGE_H

#include <KisResourceStorage.h>
#include <kritaflake_export.h>
#include <KisStoragePlugin.h>

class KRITAFLAKE_EXPORT KoFontStorage: public KisStoragePlugin
{
public:
    KoFontStorage(const PkString &location = PkString("fontregistry"));
    virtual ~KoFontStorage();

    KisResourceStorage::ResourceItem resourceItem(const PkString &url) override;
    KoResourceSP resource(const PkString &url) override;

    bool supportsVersioning() const override;
    PkSharedPointer<KisResourceStorage::ResourceIterator> resources(const PkString &resourceType) override;
    PkSharedPointer<KisResourceStorage::TagIterator> tags(const PkString &resourceType) override;

    bool isValid() const override;

    bool loadVersionedResource(KoResourceSP resource) override;
};

#endif // KOFONTSTORAGE_H
