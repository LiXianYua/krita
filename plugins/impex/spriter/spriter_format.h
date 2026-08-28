/*
 * SPDX-FileCopyrightText: 2026 S-09-c
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SPRITER_FORMAT_H
#define SPRITER_FORMAT_H

#include <PkList.h>
#include <PkSharedPointer.h>
#include <PkString.h>

#include <filesystem>
#include <system_error>

class PkStream;
class PkXmlDocument;

inline constexpr bool SpriterUsesManagerStream = true;

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

    ~Bone()
    {
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

std::filesystem::path spriterOutputDirectory(const std::filesystem::path &destination,
                                             std::error_code &error);
bool buildSpriterScml(PkXmlDocument &document,
                      const PkString &generatorVersion,
                      const PkString &entityName,
                      const PkList<Folder> &folders,
                      const Bone *rootBone,
                      const PkList<SpriterObject> &objects);
bool writeSpriterScml(PkStream *device, const PkXmlDocument &document);

#endif
