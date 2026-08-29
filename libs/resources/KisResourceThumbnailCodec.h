/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#pragma once

#include <PkAuxTypes.h>
#include <PkImage.h>
#include <PkMap.h>
#include <PkString.h>

#include <kritaresources_export.h>

namespace KisResourceThumbnailCodec
{

struct KRITARESOURCES_EXPORT PngPayload {
    PkImage image;
    PkMap<PkString, PkString> text;
};

KRITARESOURCES_EXPORT PkImage loadPng(const PkString &path);
KRITARESOURCES_EXPORT bool decodePng(const PkByteArray &data, PngPayload &payload);
KRITARESOURCES_EXPORT PkImage decodePng(const PkByteArray &data);
KRITARESOURCES_EXPORT PkByteArray encodePng(
    const PkImage &image, const PkMap<PkString, PkString> &text);
KRITARESOURCES_EXPORT PkByteArray encodePng(const PkImage &image);
KRITARESOURCES_EXPORT bool savePng(const PkString &path, const PkImage &image);

}
