/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <PkAuxTypes.h>
#include <PkImage.h>
#include <PkString.h>

namespace ImageShapePngData
{

PkString encodeBase64(const PkImage &image);
PkString encodeDataUri(const PkImage &image);
PkByteArray decodeBase64(const PkString &encoded);
PkImage decodePng(const PkByteArray &encodedPng);

} // namespace ImageShapePngData
