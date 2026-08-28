/*
 * SPDX-FileCopyrightText: 2026 S-09-c
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "spriter_format.h"

#include <PkFileStream.h>
#include <PkStream.h>
#include <PkXmlDocument.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{

class OutputStream final : public PkStream
{
public:
    const std::vector<char> &bytes() const { return m_bytes; }
    pk_int64 size() const override { return static_cast<pk_int64>(m_bytes.size()); }
    bool isSequential() const override { return false; }

protected:
    pk_int64 readData(char *, pk_int64) override { return 0; }

    pk_int64 writeData(const char *data, pk_int64 size) override
    {
        const pk_int64 offset = pos();
        const pk_int64 required = offset + size;
        if (required > static_cast<pk_int64>(m_bytes.size())) {
            m_bytes.resize(static_cast<std::size_t>(required));
        }
        std::memcpy(m_bytes.data() + offset, data, static_cast<std::size_t>(size));
        return size;
    }

private:
    std::vector<char> m_bytes;
};

std::string readFile(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

bool exportWithManagerContract(const std::filesystem::path &destination,
                               const PkXmlDocument &document)
{
    const std::filesystem::path temporary = destination.u8string() + ".tmp_test";
    PkFileStream stream(PkString(temporary.u8string().c_str()));
    if (SpriterUsesManagerStream &&
        !stream.open(PkStream::WriteOnly | PkStream::Truncate)) {
        return false;
    }

    const bool converted = writeSpriterScml(&stream, document);
    if (SpriterUsesManagerStream) {
        stream.close();
        std::error_code error;
        if (!converted) {
            std::filesystem::remove(temporary, error);
            return false;
        }
        std::filesystem::rename(temporary, destination, error);
        return !error;
    }
    return converted;
}

} // namespace

int main()
{
    if (!SpriterUsesManagerStream) {
        std::cerr << "Spriter must write through the manager-owned stream\n";
        return 1;
    }

    std::error_code pathError;
    const std::filesystem::path currentDirectory = std::filesystem::current_path(pathError);
    if (pathError || spriterOutputDirectory(std::filesystem::path("scene.scml"), pathError) != currentDirectory) {
        std::cerr << "relative Spriter destination did not resolve against the current directory\n";
        return 2;
    }

    Folder folder;
    folder.id = 0;
    folder.name = "角色";
    SpriterFile file;
    file.id = 0;
    file.name = "角色/body.png";
    file.baseName = "body";
    file.width = 12;
    file.height = 8;
    folder.files.append(file);

    PkList<Folder> folders;
    folders.append(folder);

    Bone rootBone;
    rootBone.id = 0;
    rootBone.name = "root";
    rootBone.fixLocalX = 1;
    rootBone.fixLocalY = 2;
    rootBone.fixLocalAngle = 3;
    rootBone.fixLocalScaleX = 1;
    rootBone.fixLocalScaleY = 1;

    SpriterObject object;
    object.id = 0;
    object.folderId = 0;
    object.fileId = 0;
    object.bone = &rootBone;
    object.fixLocalX = 4;
    object.fixLocalY = 5;
    object.fixLocalAngle = std::acos(-1.0) / 2.0;
    object.fixLocalScaleX = 1;
    object.fixLocalScaleY = 1;
    PkList<SpriterObject> objects;
    objects.append(object);

    PkXmlDocument document;
    if (!buildSpriterScml(document, "test-version", "画布", folders, &rootBone, objects)) {
        std::cerr << "production SCML builder rejected representative input\n";
        return 3;
    }

    OutputStream output;
    if (!output.open(PkStream::WriteOnly)) {
        return 4;
    }
    if (!writeSpriterScml(&output, document)) {
        return 5;
    }

    const std::string expected =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<spriter_data scml_version=\"1\" generator=\"krita\" generator_version=\"test-version\">\n"
        "    <folder id=\"0\" name=\"角色\">\n"
        "        <file id=\"0\" name=\"角色/body.png\" width=\"12.00\" height=\"8.00\" />\n"
        "    </folder>\n"
        "    <entity id=\"0\" name=\"画布\">\n"
        "        <animation id=\"0\" name=\"default\" length=\"1000\" looping=\"false\">\n"
        "            <mainline>\n"
        "                <key id=\"0\">\n"
        "                    <bone_ref id=\"0\" timeline=\"0\" key=\"0\" />\n"
        "                    <object_ref id=\"0\" parent=\"0\" timeline=\"1\" key=\"0\" z_index=\"0\" />\n"
        "                </key>\n"
        "            </mainline>\n"
        "            <timeline id=\"0\" name=\"root\" object_type=\"bone\">\n"
        "                <key id=\"0\" spin=\"0\">\n"
        "                    <bone x=\"1.00\" y=\"2.00\" angle=\"3.00\" scale_x=\"1.00\" scale_y=\"1.00\" />\n"
        "                </key>\n"
        "            </timeline>\n"
        "            <timeline id=\"1\" name=\"object-body\">\n"
        "                <key id=\"0\" spin=\"0\">\n"
        "                    <object folder=\"0\" file=\"0\" x=\"4\" y=\"5\" angle=\"90.00\" scale_x=\"1.00\" scale_y=\"1.00\" />\n"
        "                </key>\n"
        "            </timeline>\n"
        "        </animation>\n"
        "    </entity>\n"
        "</spriter_data>\n";

    const std::string actual(output.bytes().begin(), output.bytes().end());
    if (actual != expected) {
        std::cerr << "expected:\n" << expected << "actual:\n" << actual;
        return 6;
    }

    const std::filesystem::path managerDestination =
        std::filesystem::temp_directory_path() /
        (std::string("spriter-manager-contract-") +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".scml");
    std::error_code cleanupError;
    std::filesystem::remove(managerDestination, cleanupError);
    if (!exportWithManagerContract(managerDestination, document) ||
        readFile(managerDestination) != expected) {
        std::cerr << "manager contract did not commit a new Spriter destination\n";
        return 7;
    }

    {
        std::ofstream old(managerDestination, std::ios::binary | std::ios::trunc);
        old << "old content that must not survive replacement";
    }
    if (!exportWithManagerContract(managerDestination, document) ||
        readFile(managerDestination) != expected) {
        std::cerr << "manager contract did not replace the previous Spriter destination\n";
        return 8;
    }
    std::filesystem::remove(managerDestination, cleanupError);
    return 0;
}
