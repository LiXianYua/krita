/*
 * SPDX-FileCopyrightText: 2026 S-09-c
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "exr_import_policy.h"

#include <PkConfigGroup.h>
#include <PkSharedConfig.h>
#include <PkXmlDocument.h>

#include <iostream>
#include <set>
#include <string>

int main()
{
    PkConfigGroup config = PkSharedConfig::openConfig()->group(PkString());
    config.deleteEntry("ExrDefaultColorProfile");
    if (preferredExrColorProfile("registry-fallback") != "registry-fallback") {
        std::cerr << "missing EXR preference did not use registry fallback\n";
        return 1;
    }
    config.writeEntry("ExrDefaultColorProfile", PkString("preferred-profile"));
    if (preferredExrColorProfile("registry-fallback") != "preferred-profile") {
        std::cerr << "persisted EXR profile preference was ignored\n";
        return 2;
    }

    PkXmlDocument absentDocument;
    if (hasUsableExrLayersMetadata(false, absentDocument)) {
        std::cerr << "EXR without EXR_KRITA_LAYERS entered metadata path\n";
        return 3;
    }

    PkXmlDocument malformed;
    malformed.setContent("not xml");
    if (hasUsableExrLayersMetadata(true, malformed)) {
        std::cerr << "malformed EXR layer metadata entered sorting path\n";
        return 4;
    }

    PkXmlDocument valid;
    valid.setContent("<root><layer exr_name=\"paint\"/></root>");
    if (!hasUsableExrLayersMetadata(true, valid)) {
        std::cerr << "valid EXR layer metadata was rejected\n";
        return 5;
    }

    if (classifyExrChannels({"Y"}) != ExrChannelModel::Gray ||
        classifyExrChannels({"Y", "A"}) != ExrChannelModel::Gray ||
        classifyExrChannels({"R", "G", "B"}) != ExrChannelModel::Rgb ||
        classifyExrChannels({"X", "Y", "Z", "A"}) != ExrChannelModel::Xyz ||
        classifyExrChannels({}) != ExrChannelModel::Unsupported) {
        std::cerr << "EXR channel model classification changed\n";
        return 6;
    }

    config.deleteEntry("ExrDefaultColorProfile");
    return 0;
}
