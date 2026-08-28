/*
 *  SPDX-FileCopyrightText: 2016 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_spriter_export.h"
#include "../kis_impex_static_registration.h"
#include "spriter_format.h"

#include <PkXmlDocument.h>

#include <cmath>
#include <filesystem>
#include <string>
#include <system_error>
#include <KoColorSpaceConstants.h>
#include <KoColorSpaceRegistry.h>

#include <KisExportCheckRegistry.h>
#include <KisImportExportManager.h>

#include <KisDocument.h>
#include <kis_group_layer.h>
#include <kis_image.h>
#include <kis_layer.h>
#include <kis_node.h>
#include <kis_painter.h>
#include <kis_paint_layer.h>
#include <kis_shape_layer.h>
#include <kis_file_layer.h>
#include <kis_clone_layer.h>
#include <kis_generator_layer.h>
#include <kis_adjustment_layer.h>
#include <kis_types.h>
#include <KisPngCodec.h>
#include <kis_fast_math.h>
#include <math.h>
#include <kis_dom_utils.h>
#include <kis_layer_utils.h>
#include <kritaversion.h>

namespace
{

PkString firstWord(const PkString &value)
{
    const std::string utf8 = value.PkToUtf8();
    const std::size_t separator = utf8.find(' ');
    return PkString((separator == std::string::npos ? utf8 : utf8.substr(0, separator)).c_str());
}

PkString markerValue(const PkString &value, const char *marker)
{
    const std::string utf8 = value.PkToUtf8();
    const std::string prefix(marker);
    const std::size_t start = utf8.find(prefix);
    if (start == std::string::npos) {
        return PkString();
    }
    const std::size_t contentStart = start + prefix.size();
    const std::size_t end = utf8.find(')', contentStart);
    if (end == std::string::npos) {
        return PkString();
    }
    return PkString(utf8.substr(contentStart, end - contentStart).c_str());
}

PkString pkPath(const std::filesystem::path &path)
{
    return PkString(path.u8string().c_str());
}

} // namespace

extern "C" KRITAIMPEX_EXPORT void registerKisSpriterExportFilter()
{
    static bool registered = false;
    registerKisImpexFilterOnce(
        registered, {}, {PkString("application/x-spriter")}, 1,
        []() -> KisImportExportFilter * { return new KisSpriterExport(nullptr, PkVariantList()); });
}

KisSpriterExport::KisSpriterExport(PkObject *parent, const PkVariantList &) : KisImportExportFilter(parent)
{
}

KisSpriterExport::~KisSpriterExport()
{
    delete m_rootBone;
}

KisImportExportErrorCode KisSpriterExport::savePaintDevice(KisPaintDeviceSP dev, const PkString &fileName)
{
    const std::filesystem::path path = std::filesystem::u8path(fileName.PkToUtf8());
    std::error_code filesystemError;
    std::filesystem::create_directories(path.parent_path(), filesystemError);
    if (filesystemError) {
        return ImportExportCodes::CannotCreateFile;
    }
    PkRect rc = m_image->bounds().intersected(dev->exactBounds());

    if (!KisPngCodec::isColorSpaceSupported(dev->colorSpace())) {
        dev = new KisPaintDevice(*dev.data());
        dev->convertTo(KoColorSpaceRegistry::instance()->rgb8());
    }

    KisPNGOptions options;
    options.forceSRGB = true;

    vKisAnnotationSP_it beginIt = m_image->beginAnnotations();
    vKisAnnotationSP_it endIt = m_image->endAnnotations();

    KisPngCodec converter;
    KisImportExportErrorCode res = converter.buildFile(pkPath(path), rc, m_image->xRes(), m_image->yRes(), dev, beginIt, endIt, options, 0);

    return res;
}

KisImportExportErrorCode KisSpriterExport::parseFolder(KisGroupLayerSP parentGroup, const PkString &folderName, const PkString &basePath, int *folderId)
{
    int currentFolder=0;
	if(folderId == 0)
	{
		folderId = &currentFolder;
	}
    PkString pathName;
    if (!folderName.isEmpty()) {
        pathName = folderName + "/";
    }


    KisNodeSP child = parentGroup->lastChild();
    while (child) {
        if (child->visible() && child->inherits("KisGroupLayer")) {
            KisImportExportErrorCode res = parseFolder(
                child.dynamicCast<KisGroupLayer>(),
                firstWord(child->name()),
                basePath + "/" + pathName,
                folderId);
            if (!res.isOk()) {
                return res;
            }
        }
        child = child->prevSibling();
    }

    Folder folder;
    folder.id = *folderId;
    folder.name = folderName;
    folder.groupName = parentGroup->name();

    int fileId = 0;
    child = parentGroup->lastChild();

    while (child) {
        if (child->visible() && !child->inherits("KisGroupLayer") && !child->inherits("KisMask")) {
            PkRect rc = m_image->bounds().intersected(child->exactBounds());
            PkString layerBaseName = firstWord(child->name());
            SpriterFile file;
            file.id = fileId++;
            file.pathName = pathName;
            file.baseName = layerBaseName;
            file.layerName = child->name();
            file.name = folderName + "/" + layerBaseName + ".png";

            qreal xmin = rc.left();
            qreal ymin = rc.top();
            qreal xmax = rc.right();
            qreal ymax = rc.bottom();

            file.width = xmax - xmin;
            file.height = ymax - ymin;
            file.x = xmin;
            file.y = ymin;
            KisImportExportErrorCode result = savePaintDevice(child->projection(), basePath + file.name);
            if (result.isOk()) {
                folder.files.append(file);
            } else {
                return result;
            }
        }

        child = child->prevSibling();
    }

    if (folder.files.size() > 0) {
        m_folders.append(folder);
        (*folderId)++;
    }

    return ImportExportCodes::OK;
}

Bone *KisSpriterExport::parseBone(const Bone *parent, KisGroupLayerSP groupLayer)
{
    PkString groupBaseName = firstWord(groupLayer->name());
    Bone *bone = new Bone;
    bone->id = m_nextBoneId++;
    bone->parentBone = parent;
    bone->name = groupBaseName;

    if (m_boneLayer) {
        PkRect rc = m_image->bounds().intersected(m_boneLayer->exactBounds());

        qreal xmin = rc.left();
        qreal ymin = rc.top();
        qreal xmax = rc.right();
        qreal ymax = rc.bottom();

        bone->x = (xmin + xmax) / 2;
        bone->y = -(ymin + ymax) / 2;
        bone->width = xmax - xmin;
        bone->height = ymax - ymin;
    }
    else {
        bone->x = 0.0;
        bone->y = 0.0;
        bone->width = 0.0;
        bone->height = 0.0;
    }

    if (parent) {
        bone->localX = bone->x - parent->x;
        bone->localY = bone->y - parent->y;
    }
    else {
        bone->localX = bone->x;
        bone->localY = bone->y;
    }

    bone->localAngle = 0.0;
    bone->localScaleX = 1.0;
    bone->localScaleY = 1.0;

    KisNodeSP child = groupLayer->lastChild();
    while (child) {
        if (child->visible() && child->inherits("KisGroupLayer")) {
            bone->bones.append(parseBone(bone, child.dynamicCast<KisGroupLayer>()));
        }
        child = child->prevSibling();
    }

    return bone;
}

void copyBone(Bone *startBone)
{
    startBone->fixLocalX = startBone->localX;
    startBone->fixLocalY = startBone->localY;
    startBone->fixLocalAngle = startBone->localAngle;
    startBone->fixLocalScaleX= startBone->localScaleX;
    startBone->fixLocalScaleY= startBone->localScaleY;

    for (Bone *child : startBone->bones) {
        copyBone(child);
    }
}

void KisSpriterExport::fixBone(Bone *bone)
{
    double boneLocalAngle = 0;
    double boneLocalScaleX = 1;

    if (bone->bones.length() >= 1) {
        // if a bone has one or more children, point at first child
        Bone *childBone = bone->bones[0];
        double dx = childBone->x - bone->x;
        double dy = childBone->y - bone->y;
        if (std::abs(dx) > 0 || std::abs(dy) > 0) {
            boneLocalAngle = KisFastMath::atan2(dy, dx);
            boneLocalScaleX = sqrt(dx * dx + dy * dy) / 200;
        }
    }
    else if (bone->parentBone) {
        // else, if bone has parent, point away from parent
        double dx = bone->x - bone->parentBone->x;
        double dy = bone->y - bone->parentBone->y;
        if (std::abs(dx) > 0 || std::abs(dy) > 0) {
            boneLocalAngle = KisFastMath::atan2(dy, dx);
            boneLocalScaleX = sqrt(dx * dx + dy * dy) / 200;
        }
    }
    // adjust bone angle
    bone->fixLocalAngle += boneLocalAngle;
    bone->fixLocalScaleX *= boneLocalScaleX;

    // rotate all the child bones back to world position
    for (int i = 0; i < bone->bones.length(); ++i) {
        Bone *childBone = bone->bones[i];

        double tx = childBone->fixLocalX;
        double ty = childBone->fixLocalY;

        childBone->fixLocalX = tx * cos(-boneLocalAngle) - ty * sin(-boneLocalAngle);
        childBone->fixLocalY = tx * sin(-boneLocalAngle) + ty * cos(-boneLocalAngle);

        childBone->fixLocalX /= boneLocalScaleX;
        childBone->fixLocalAngle -= boneLocalAngle;
        childBone->fixLocalScaleX /= boneLocalScaleX;
    }

    // rotate all the child objects back to world position
    for (int i = 0; i < m_objects.length(); ++i) {
        if (m_objects[i].bone == bone) {
            m_objects[i].fixLocalAngle -= boneLocalAngle;
            m_objects[i].fixLocalScaleX /= boneLocalScaleX;
        }
    }

    // process all child bones
    for (int i = 0; i < bone->bones.length(); ++i) {
        fixBone(bone->bones[i]);
    }
}

Bone *findBoneByName(Bone *startBone, const PkString &name)
{
    if (!startBone) return 0;

    if (startBone->name == name) {
        return startBone;
    }
    for (Bone *child : startBone->bones) {
        if (child->name == name) {
            return child;
        }
        Bone *grandChild = findBoneByName(child, name);
        if (grandChild){
            return grandChild;
        }
    }
    return 0;
}

KisImportExportErrorCode KisSpriterExport::convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP /*configuration*/)
{
    if (!document || !io) {
        return ImportExportCodes::InternalError;
    }

    const std::filesystem::path outputPath = std::filesystem::u8path(filename().PkToUtf8());
    std::error_code pathError;
    const std::filesystem::path outputDirectory = spriterOutputDirectory(outputPath, pathError);
    if (pathError || outputDirectory.empty()) {
        return ImportExportCodes::CannotCreateFile;
    }

    m_image = document->savingImage();
    KIS_ASSERT_RECOVER_RETURN_VALUE(m_image, ImportExportCodes::InternalError);

    delete m_rootBone;
    m_rootBone = nullptr;
    m_folders.clear();
    m_objects.clear();
    m_nextBoneId = 0;

    if (m_image->rootLayer()->childCount() == 0) {
        return ImportExportCodes::Failure;
    }

    KisGroupLayerSP root = m_image->rootLayer();

    m_boneLayer = KisLayerUtils::findNodeByName(root,"bone").dynamicCast<KisLayer>();

    m_rootLayer= KisLayerUtils::findNodeByName(root,"root").dynamicCast<KisGroupLayer>();

    KisImportExportErrorCode result =
        parseFolder(m_image->rootLayer(), "", pkPath(outputDirectory));
    if (!result.isOk()) {
        dbgFile << "There were errors encountered while using the spriter exporter.";
        return result;
    }

    m_rootBone = 0;

    if (m_rootLayer) {
        m_rootBone = parseBone(0, m_rootLayer);
    }
    // Generate objects
    int objectId = 0;
    for (int folderIndex = 0, folderCount = m_folders.size(); folderIndex < folderCount; ++folderIndex) {
        Folder folder = m_folders[folderCount - 1 - folderIndex];
        for (int fileIndex = 0, fileCount = folder.files.size(); fileIndex < fileCount; ++ fileIndex) {
            SpriterFile file = folder.files[fileCount - 1 - fileIndex];
            SpriterObject spriterObject;
            spriterObject.id = objectId++;
            spriterObject.folderId = folder.id;
            spriterObject.fileId = file.id;
            spriterObject.x = file.x;
            spriterObject.y = -file.y;
            Bone *bone = 0;

            // layer.name format: "base_name bone(bone_name) slot(slot_name)"
            if (file.layerName.contains("bone(")) {
                PkString boneName = markerValue(file.layerName, "bone(");
                bone = findBoneByName(m_rootBone, boneName);
            }


            // layer.name format: "base_name"
            if (!bone && m_rootBone) {
                bone = findBoneByName(m_rootBone, file.layerName);
            }
            // group.name format: "base_name bone(bone_name)"
            if (!bone && m_rootBone) {
                if (folder.groupName.contains("bone(")) {
                    PkString boneName = markerValue(folder.groupName, "bone(");
                    bone = findBoneByName(m_rootBone, boneName);
                }

                // group.name format: "base_name"
                if (!bone) {
                    bone = findBoneByName(m_rootBone, folder.groupName);
                }
            }

            if (!bone) {
                bone = m_rootBone;
            }

            if (bone) {
                spriterObject.bone = bone;
                spriterObject.localX = spriterObject.x - bone->x;
                spriterObject.localY = spriterObject.y - bone->y;
            }
            else {
                spriterObject.bone = 0;
                spriterObject.localX = spriterObject.x;
                spriterObject.localY = spriterObject.y;
            }

            spriterObject.localAngle = 0;
            spriterObject.localScaleX = 1.0;
            spriterObject.localScaleY = 1.0;

            PkSharedPointer<SpriterSlot> slot;

            // layer.name format: "base_name bone(bone_name) slot(slot_name)"
            if (file.layerName.contains("slot(")) {
                slot = PkSharedPointer<SpriterSlot>(new SpriterSlot());
                slot->name = markerValue(file.layerName, "slot(");
                slot->defaultAttachmentFlag = file.layerName.contains("*");
            }

            spriterObject.slot = slot;

            m_objects.append(spriterObject);
        }
    }

    // Copy object transforms
    for (int i = 0; i < m_objects.size(); ++i) {
        m_objects[i].fixLocalX = m_objects[i].localX;
        m_objects[i].fixLocalY = m_objects[i].localY;
        m_objects[i].fixLocalAngle = m_objects[i].localAngle;
        m_objects[i].fixLocalScaleX = m_objects[i].localScaleX;
        m_objects[i].fixLocalScaleY = m_objects[i].localScaleY;
    }

    // Calculate bone angles
    if (m_rootBone) {
        copyBone(m_rootBone);
        fixBone(m_rootBone);
    }

    // Generate scml
    PkXmlDocument scml;
    if (!buildSpriterScml(scml,
                          KRITA_VERSION_STRING,
                          PkString(outputPath.stem().u8string().c_str()),
                          m_folders,
                          m_rootBone,
                          m_objects)) {
        delete m_rootBone;
        m_rootBone = nullptr;
        return ImportExportCodes::InternalError;
    }

    const bool written = writeSpriterScml(io, scml);

    delete m_rootBone;
    m_rootBone = nullptr;

    return written ? KisImportExportErrorCode(ImportExportCodes::OK)
                   : KisImportExportErrorCode(ImportExportCodes::ErrorWhileWriting);
}

void KisSpriterExport::initializeCapabilities()
{
    addCapability(KisExportCheckRegistry::instance()->get("MultiLayerCheck")->create(KisExportCheckBase::SUPPORTED));
    addCapability(KisExportCheckRegistry::instance()->get("LayerOpacityCheck")->create(KisExportCheckBase::PARTIALLY));
    PkList<std::pair<KoID, KoID> > supportedColorModels;
    supportedColorModels << std::pair<KoID, KoID>()
            << std::pair<KoID, KoID>(RGBAColorModelID, Integer8BitsColorDepthID);
    addSupportedColorModels(supportedColorModels, "Spriter");
}
