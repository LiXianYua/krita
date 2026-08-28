/*
 * SPDX-FileCopyrightText: 2026 S-09-c
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "spriter_format.h"

#include <PkStream.h>
#include <PkString.h>
#include <PkXmlDocument.h>

#include <cmath>
#include <filesystem>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>

namespace
{

PkString formatNumber(double value)
{
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(6) << std::defaultfloat << value;
    return PkString(stream.str().c_str());
}

PkString formatFixed(double value)
{
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::fixed << std::setprecision(2) << value;
    return PkString(stream.str().c_str());
}

void writeBoneRef(const Bone *bone,
                  PkXmlElement &key,
                  PkXmlDocument &document,
                  double &timelineId)
{
    if (!bone) {
        return;
    }
    PkXmlElement boneRef = document.createElement("bone_ref");
    key.appendChild(boneRef);
    boneRef.setAttribute("id", formatNumber(bone->id));
    if (bone->parentBone) {
        boneRef.setAttribute("parent", formatNumber(bone->parentBone->id));
    }
    boneRef.setAttribute("timeline", formatNumber(timelineId++));
    boneRef.setAttribute("key", "0");
    for (const Bone *childBone : bone->bones) {
        writeBoneRef(childBone, key, document, timelineId);
    }
}

void writeBone(const Bone *bone,
               PkXmlElement &animation,
               PkXmlDocument &document,
               double &timelineId)
{
    if (!bone) {
        return;
    }
    PkXmlElement timeline = document.createElement("timeline");
    animation.appendChild(timeline);
    timeline.setAttribute("id", formatNumber(timelineId));
    timeline.setAttribute("name", bone->name);
    timeline.setAttribute("object_type", "bone");

    PkXmlElement key = document.createElement("key");
    timeline.appendChild(key);
    key.setAttribute("id", "0");
    key.setAttribute("spin", "0");

    PkXmlElement boneElement = document.createElement("bone");
    key.appendChild(boneElement);
    boneElement.setAttribute("x", formatFixed(bone->fixLocalX));
    boneElement.setAttribute("y", formatFixed(bone->fixLocalY));
    boneElement.setAttribute("angle", formatFixed(bone->fixLocalAngle));
    boneElement.setAttribute("scale_x", formatFixed(bone->fixLocalScaleX));
    boneElement.setAttribute("scale_y", formatFixed(bone->fixLocalScaleY));

    ++timelineId;
    for (const Bone *childBone : bone->bones) {
        writeBone(childBone, animation, document, timelineId);
    }
}

bool writeAll(PkStream *device, const std::string &bytes)
{
    PkStream::pk_int64 written = 0;
    const PkStream::pk_int64 size = static_cast<PkStream::pk_int64>(bytes.size());
    while (written < size) {
        const PkStream::pk_int64 chunk = device->write(bytes.data() + written, size - written);
        if (chunk <= 0) {
            return false;
        }
        written += chunk;
    }
    return true;
}

} // namespace

std::filesystem::path spriterOutputDirectory(const std::filesystem::path &destination,
                                             std::error_code &error)
{
    const std::filesystem::path absoluteDestination = std::filesystem::absolute(destination, error);
    return error ? std::filesystem::path() : absoluteDestination.parent_path();
}

bool buildSpriterScml(PkXmlDocument &document,
                      const PkString &generatorVersion,
                      const PkString &entityName,
                      const PkList<Folder> &folders,
                      const Bone *rootBone,
                      const PkList<SpriterObject> &objects)
{
    PkXmlElement root = document.createElement("spriter_data");
    document.appendChild(root);
    root.setAttribute("scml_version", "1");
    root.setAttribute("generator", "krita");
    root.setAttribute("generator_version", generatorVersion);

    for (const Folder &folder : folders) {
        PkXmlElement folderElement = document.createElement("folder");
        root.appendChild(folderElement);
        folderElement.setAttribute("id", formatNumber(folder.id));
        folderElement.setAttribute("name", folder.name);
        for (const SpriterFile &file : folder.files) {
            PkXmlElement fileElement = document.createElement("file");
            folderElement.appendChild(fileElement);
            fileElement.setAttribute("id", formatNumber(file.id));
            fileElement.setAttribute("name", file.name);
            fileElement.setAttribute("width", formatFixed(file.width));
            fileElement.setAttribute("height", formatFixed(file.height));
        }
    }

    PkXmlElement entity = document.createElement("entity");
    root.appendChild(entity);
    entity.setAttribute("id", "0");
    entity.setAttribute("name", entityName);

    PkXmlElement animation = document.createElement("animation");
    entity.appendChild(animation);
    animation.setAttribute("id", "0");
    animation.setAttribute("name", "default");
    animation.setAttribute("length", "1000");
    animation.setAttribute("looping", "false");

    PkXmlElement mainline = document.createElement("mainline");
    animation.appendChild(mainline);
    PkXmlElement key = document.createElement("key");
    mainline.appendChild(key);
    key.setAttribute("id", "0");

    double timelineId = 0;
    writeBoneRef(rootBone, key, document, timelineId);
    for (const SpriterObject &object : objects) {
        PkXmlElement objectRef = document.createElement("object_ref");
        key.appendChild(objectRef);
        objectRef.setAttribute("id", formatNumber(object.id));
        if (object.bone) {
            objectRef.setAttribute("parent", formatNumber(object.bone->id));
        }
        objectRef.setAttribute("timeline", formatNumber(timelineId++));
        objectRef.setAttribute("key", "0");
        objectRef.setAttribute("z_index", formatNumber(object.id));
    }

    timelineId = 0;
    writeBone(rootBone, animation, document, timelineId);

    for (const SpriterObject &object : objects) {
        const SpriterFile *matchingFile = nullptr;
        for (const Folder &folder : folders) {
            if (folder.id != object.folderId) {
                continue;
            }
            for (const SpriterFile &file : folder.files) {
                if (file.id == object.fileId) {
                    matchingFile = &file;
                    break;
                }
            }
            break;
        }
        if (!matchingFile) {
            return false;
        }

        PkXmlElement timeline = document.createElement("timeline");
        animation.appendChild(timeline);
        timeline.setAttribute("id", formatNumber(timelineId++));
        timeline.setAttribute("name", PkString("object-") + matchingFile->baseName);

        PkXmlElement objectKey = document.createElement("key");
        timeline.appendChild(objectKey);
        objectKey.setAttribute("id", "0");
        objectKey.setAttribute("spin", "0");

        PkXmlElement objectElement = document.createElement("object");
        objectKey.appendChild(objectElement);
        objectElement.setAttribute("folder", formatNumber(object.folderId));
        objectElement.setAttribute("file", formatNumber(object.fileId));
        objectElement.setAttribute("x", formatNumber(object.fixLocalX));
        objectElement.setAttribute("y", formatNumber(object.fixLocalY));
        const double degrees = object.fixLocalAngle * 180.0 / std::acos(-1.0);
        objectElement.setAttribute("angle", formatFixed(degrees));
        objectElement.setAttribute("scale_x", formatFixed(object.fixLocalScaleX));
        objectElement.setAttribute("scale_y", formatFixed(object.fixLocalScaleY));
    }
    return true;
}

bool writeSpriterScml(PkStream *device, const PkXmlDocument &document)
{
    if (!device) {
        return false;
    }

    bool openedHere = false;
    if (!device->isOpen()) {
        openedHere = device->open(PkStream::WriteOnly);
        if (!openedHere) {
            return false;
        }
    }

    const std::string bytes =
        std::string("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n") +
        document.toString(4).PkToUtf8();
    const bool ok = writeAll(device, bytes);

    if (openedHere) {
        device->close();
    }
    return ok;
}
