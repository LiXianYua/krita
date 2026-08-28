/*
 *  SPDX-FileCopyrightText: 2016 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _KIS_SPRITER_EXPORT_H_
#define _KIS_SPRITER_EXPORT_H_

#include <PkVariant.h>
#include <PkXmlDocument.h>
#include <PkList.h>
#include <PkSharedPointer.h>

#include <KisImportExportFilter.h>
#include <kis_types.h>

struct SpriterFile {
    double id {0.0};
    PkString name;
    PkString pathName;
    PkString baseName;
    PkString layerName;
    double width {0.0};
    double height {0.0};
    double x {0.0};
    double y {0.0};
};

struct Folder {
    double id {0.0};
    PkString name;
    PkString pathName;
    PkString baseName;
    PkString groupName;
    PkList<SpriterFile> files;
};

struct Bone {
    double id {0.0};
    const Bone *parentBone {nullptr};
    PkString name;
    double x {0.0};
    double y {0.0};
    double width {0.0};
    double height {0.0};
    double localX {0.0};
    double localY {0.0};
    double localAngle {0.0};
    double localScaleX {0.0};
    double localScaleY {0.0};
    double fixLocalX {0.0};
    double fixLocalY {0.0};
    double fixLocalAngle {0.0};
    double fixLocalScaleX {0.0};
    double fixLocalScaleY {0.0};
    PkList<Bone*> bones;

    ~Bone() {
        for (Bone *bone : bones) {
            delete bone;
        }
        bones.clear();
    }
};

struct SpriterSlot {
    PkString name;
    bool defaultAttachmentFlag = false;
};

struct SpriterObject {
    double id {0.0};
    double folderId {0.0};
    double fileId {0.0};
    Bone *bone {nullptr};
    PkSharedPointer<SpriterSlot> slot;
    double x {0.0};
    double y {0.0};
    double localX {0.0};
    double localY {0.0};
    double localAngle {0.0};
    double localScaleX {0.0};
    double localScaleY {0.0};
    double fixLocalX {0.0};
    double fixLocalY {0.0};
    double fixLocalAngle {0.0};
    double fixLocalScaleX {0.0};
    double fixLocalScaleY {0.0};

};

class KisSpriterExport : public KisImportExportFilter
{
    Q_OBJECT
public:
    KisSpriterExport(PkObject *parent, const PkVariantList &);
    ~KisSpriterExport() override;
    bool supportsIO() const override { return false; }
    KisImportExportErrorCode convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration = 0) override;
    void initializeCapabilities() override;
private:

    KisImportExportErrorCode savePaintDevice(KisPaintDeviceSP dev, const PkString &fileName);
    KisImportExportErrorCode parseFolder(KisGroupLayerSP parentGroup, const PkString &folderName, const PkString &basePath, int *folderId = 0);
    Bone *parseBone(const Bone *parent, KisGroupLayerSP groupLayer);
    void fixBone(Bone *bone);
    void fillScml(PkXmlDocument &scml, const PkString &entityName);
    void writeBoneRef(const Bone *bone, PkXmlElement &mainline, PkXmlDocument &scml);
    void writeBone(const Bone *bone, PkXmlElement &timeline, PkXmlDocument &scml);

    KisImageSP m_image;
    double m_timelineid {0.0};
    int m_nextBoneId {0};
    PkList<Folder> m_folders;
    Bone *m_rootBone {nullptr};
    PkList<SpriterObject> m_objects;
    KisGroupLayerSP m_rootLayer; // Not the image's root later, but the one that is named "root"
    KisLayerSP m_boneLayer;

};

#endif
