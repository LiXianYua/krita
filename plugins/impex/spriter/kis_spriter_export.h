/*
 *  SPDX-FileCopyrightText: 2016 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _KIS_SPRITER_EXPORT_H_
#define _KIS_SPRITER_EXPORT_H_

#include <PkVariant.h>

#include <KisImportExportFilter.h>
#include <kis_types.h>

#include "spriter_format.h"

class KisSpriterExport : public KisImportExportFilter
{
    Q_OBJECT
public:
    KisSpriterExport(PkObject *parent, const PkVariantList &);
    ~KisSpriterExport() override;
    bool supportsIO() const override { return SpriterUsesManagerStream; }
    KisImportExportErrorCode convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration = 0) override;
    void initializeCapabilities() override;
private:

    KisImportExportErrorCode savePaintDevice(KisPaintDeviceSP dev, const PkString &fileName);
    KisImportExportErrorCode parseFolder(KisGroupLayerSP parentGroup, const PkString &folderName, const PkString &basePath, int *folderId = 0);
    Bone *parseBone(const Bone *parent, KisGroupLayerSP groupLayer);
    void fixBone(Bone *bone);
    KisImageSP m_image;
    int m_nextBoneId {0};
    PkList<Folder> m_folders;
    Bone *m_rootBone {nullptr};
    PkList<SpriterObject> m_objects;
    KisGroupLayerSP m_rootLayer; // Not the image's root later, but the one that is named "root"
    KisLayerSP m_boneLayer;

};

#endif
