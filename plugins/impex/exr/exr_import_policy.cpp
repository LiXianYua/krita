/*
 * SPDX-FileCopyrightText: 2026 S-09-c
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "exr_import_policy.h"

#include <PkSharedConfig.h>
#include <PkXmlDocument.h>

PkString preferredExrColorProfile(const PkString &registryFallback)
{
    return PkSharedConfig::openConfig()->group(PkString()).readEntry(
        "ExrDefaultColorProfile", registryFallback);
}

bool hasUsableExrLayersMetadata(bool attributePresent, const PkXmlDocument &document)
{
    const PkXmlElement root = document.documentElement();
    return attributePresent && !root.isNull() && root.hasChildNodes();
}

ExrChannelModel classifyExrChannels(const std::set<std::string> &channels)
{
    if (channels.size() == 1 || channels.size() == 2) {
        return ExrChannelModel::Gray;
    }
    if (channels.size() != 3 && channels.size() != 4) {
        return ExrChannelModel::Unsupported;
    }
    if (channels.count("R") && channels.count("G") && channels.count("B")) {
        return ExrChannelModel::Rgb;
    }
    if (channels.count("X") && channels.count("Y") && channels.count("Z")) {
        return ExrChannelModel::Xyz;
    }
    return ExrChannelModel::RemappedRgb;
}
