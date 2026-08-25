/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KISASLSTORAGE_H
#define KISASLSTORAGE_H

#include <kritaimage_export.h>
#include <PkSharedPointer.h>
#include <PkString.h>

#include <KisStoragePlugin.h>
#include <kis_asl_layer_style_serializer.h>

class KRITAIMAGE_EXPORT KisAslStorage : public KisStoragePlugin
{
public:
    KisAslStorage(const PkString &location);
    virtual ~KisAslStorage();

    KisResourceStorage::ResourceItem resourceItem(const PkString &url) override;
    KoResourceSP resource(const PkString &url) override;
    bool loadVersionedResource(KoResourceSP resource) override;
    bool supportsVersioning() const override;
    PkSharedPointer<KisResourceStorage::ResourceIterator> resources(const PkString &resourceType) override;
    PkSharedPointer<KisResourceStorage::TagIterator> tags(const PkString &resourceType) override;

    bool saveAsNewVersion(const PkString &resourceType, KoResourceSP resource) override;
    bool addResource(const PkString &resourceType, KoResourceSP resource) override;

    bool isValid() const override;

    PkSharedPointer<KisAslLayerStyleSerializer> m_aslSerializer;
};

#endif // KISASLSTORAGE_H
