/* This file is part of the KDE project
   SPDX-FileCopyrightText: 2014 Victor Lafon <metabolic.ewilan@hotmail.fr>

   SPDX-License-Identifier: LGPL-2.1-or-later
*/

#ifndef KORESOURCEBUNDLEMANIFEST_H
#define KORESOURCEBUNDLEMANIFEST_H

#include <PkList.h>
#include <PkMap.h>
#include <PkString.h>
#include <PkStringList.h>

#include <kritaresources_export.h>

class PkStream;
class PkXmlElement;

class KRITARESOURCES_EXPORT KoResourceBundleManifest
{
public:
    struct ResourceReference {
        ResourceReference() = default;

        ResourceReference(const PkString &resourcePath,
                          const PkStringList &tagList,
                          const PkString &fileTypeName,
                          const PkString &md5,
                          int resourceId = -1,
                          const PkString &filenameInBundle = PkString());

        PkString resourcePath;
        PkStringList tagList;
        PkString fileTypeName;
        PkString md5sum;
        int resourceId = -1;
        PkString filenameInBundle;
    };

    KoResourceBundleManifest();
    virtual ~KoResourceBundleManifest();

    bool load(PkStream *device);
    bool save(PkStream *device);

    void addResource(const PkString &fileType,
                     const PkString &fileName,
                     const PkStringList &tagFileList,
                     const PkString &md5,
                     int resourceId = -1,
                     const PkString &filenameInBundle = PkString());
    void removeResource(ResourceReference &resource);

    PkStringList types() const;
    PkStringList tags() const;
    PkList<ResourceReference> files(const PkString &type = PkString()) const;
    void removeFile(PkString fileName);

private:
    PkMap<PkString, PkMap<PkString, ResourceReference>> m_resources;
    bool parseFileEntry(const PkXmlElement &element);
};

#endif
