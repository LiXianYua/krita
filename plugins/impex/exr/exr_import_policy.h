/*
 * SPDX-FileCopyrightText: 2026 S-09-c
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include <PkString.h>

#include <set>
#include <string>

class PkXmlDocument;

enum class ExrChannelModel {
    Gray,
    Rgb,
    Xyz,
    RemappedRgb,
    Unsupported
};

PkString preferredExrColorProfile(const PkString &registryFallback);
bool hasUsableExrLayersMetadata(bool attributePresent, const PkXmlDocument &document);
ExrChannelModel classifyExrChannels(const std::set<std::string> &channels);
