/*
 * SPDX-FileCopyrightText: 2014 Victor Lafon metabolic.ewilan @hotmail.fr
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KORESOURCEBUNDLE_H
#define KORESOURCEBUNDLE_H

#include <PkImage.h>
#include <PkList.h>
#include <PkMap.h>
#include <PkScopedPointer.h>
#include <PkSet.h>
#include <PkSharedPointer.h>
#include <PkString.h>
#include <PkStringList.h>
#include <PkVector.h>

#include <KoResource.h>

#include "KoResourceBundleManifest.h"
#include "kritaresources_export.h"

#include <KisTag.h>

class KoStore;
class KoXmlWriter;
class PkStream;

/** A zip archive containing resources, metadata, a preview and a manifest. */
class KRITARESOURCES_EXPORT KoResourceBundle
{
public:
    explicit KoResourceBundle(const PkString &fileName);
    virtual ~KoResourceBundle();

    PkString defaultFileExtension() const;

    bool load();
    bool loadFromDevice(PkStream *device);
    bool save();
    bool saveToDevice(PkStream *device) const;

    void setMetaData(const PkString &key, const PkString &value);
    PkString metaData(const PkString &key,
                      const PkString &defaultValue = PkString()) const;

    void addResource(PkString fileType,
                     PkString filePath,
                     PkVector<KisTagSP> fileTagList,
                     const PkString &md5sum,
                     int resourceId = -1,
                     const PkString &filenameInBundle = PkString());

    PkList<PkString> getTagsList();
    void setThumbnail(PkImage image);

    void saveMetadata(PkScopedPointer<KoStore> &store);
    void saveManifest(PkScopedPointer<KoStore> &store);

    PkStringList resourceTypes() const;
    int resourceCount() const;

    KoResourceBundleManifest &manifest();

    KoResourceSP resource(const PkString &resourceType,
                          const PkString &filepath);
    bool exportResource(const PkString &resourceType,
                        const PkString &fileName,
                        PkStream *device);
    bool loadResource(KoResourceSP resource);

    PkImage image() const;
    PkString filename() const;
    PkString resourceMd5(const PkString &url);

private:
    void writeMeta(const PkString &metaTag, KoXmlWriter *writer);
    void writeUserDefinedMeta(const PkString &metaTag, KoXmlWriter *writer);
    bool readMetaData(KoStore *resourceStore);
    bool loadFromStore(KoStore *resourceStore);

    PkImage m_thumbnail;
    KoResourceBundleManifest m_manifest;
    PkMap<PkString, PkString> m_metadata;
    PkSet<PkString> m_bundletags;
    PkString m_filename;
    PkString m_bundleVersion;
};

using KoResourceBundleSP = PkSharedPointer<KoResourceBundle>;

#endif // KORESOURCEBUNDLE_H
