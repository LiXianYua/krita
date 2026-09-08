/*
 * SPDX-FileCopyrightText: 2025 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef SVGTEXTTOOLOPTIONSDATA_H
#define SVGTEXTTOOLOPTIONSDATA_H

#include <PkString.h>

struct SvgTextToolOptionsData
{
    bool useCurrentTextProperties = true;
    PkString cssStylePresetName;

    bool useVisualBidiCursor = false;

    bool pasteRichtTextByDefault = false;

    void writeConfig(const PkString &toolId);
    void loadConfig(const PkString &toolId);
    void resetConfig();
};

#endif // SVGTEXTTOOLOPTIONSDATA_H
