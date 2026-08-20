/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#pragma once

#include <PkAuxTypes.h>
#include <PkImage.h>
#include <PkString.h>

namespace KisResourceThumbnailCodec
{

PkImage loadPng(const PkString &path);
PkImage decodePng(const PkByteArray &data);
PkByteArray encodePng(const PkImage &image);
bool savePng(const PkString &path, const PkImage &image);

}
