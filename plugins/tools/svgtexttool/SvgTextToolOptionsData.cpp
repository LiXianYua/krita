/*
 * SPDX-FileCopyrightText: 2025 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "SvgTextToolOptionsData.h"

#include <PkSharedConfig.h>
#include <PkConfigGroup.h>

const PkString USE_CURRENT_TEXT_PROPERTIES = "useCurrentTextProperties";
const PkString CSS_STYLE_PRESET_NAME = "cssStylePresetName";
const PkString USE_VISUAL_BIDI_CURSOR = "useVisualBidiCursor";
const PkString PASTE_RICH_TEXT_BY_DEFAULT = "pasteRichtTextByDefault";

void SvgTextToolOptionsData::writeConfig(const PkString &toolId)
{
    PkConfigGroup configGroup = PkSharedConfig::openConfig()->group(toolId);
    configGroup.writeEntry(USE_CURRENT_TEXT_PROPERTIES, useCurrentTextProperties);
    configGroup.writeEntry(CSS_STYLE_PRESET_NAME, cssStylePresetName);
    configGroup.writeEntry(USE_VISUAL_BIDI_CURSOR, useVisualBidiCursor);
    configGroup.writeEntry(PASTE_RICH_TEXT_BY_DEFAULT, pasteRichtTextByDefault);
}

void SvgTextToolOptionsData::loadConfig(const PkString &toolId)
{
    PkConfigGroup configGroup = PkSharedConfig::openConfig()->group(toolId);
    useCurrentTextProperties = configGroup.readEntry<bool>(USE_CURRENT_TEXT_PROPERTIES, true);
    cssStylePresetName = configGroup.readEntry<PkString>(CSS_STYLE_PRESET_NAME, PkString());
    useVisualBidiCursor = configGroup.readEntry<bool>(USE_VISUAL_BIDI_CURSOR, false);
    pasteRichtTextByDefault = configGroup.readEntry<bool>(PASTE_RICH_TEXT_BY_DEFAULT, false);
}

void SvgTextToolOptionsData::resetConfig()
{
    useCurrentTextProperties = true;
    cssStylePresetName = PkString();
    useVisualBidiCursor = false;
    pasteRichtTextByDefault = false;
}
