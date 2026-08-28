/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <PkAuxTypes.h>
#include <PkImage.h>
#include <PkString.h>

#include <cstddef>

namespace ImageShapePngData
{

PkString encodeBase64(const PkImage &image);
PkString encodeDataUri(const PkImage &image);
PkByteArray decodeBase64(const PkString &encoded);
PkByteArray decodeDataUriBase64(const PkString &dataUri);
PkImage decodePng(const PkByteArray &encodedPng);
PkImage decodeImage(const PkByteArray &encodedImage);

std::size_t maxDecodedCompressedBytes();
bool base64DecodedSizeWithinLimit(std::size_t encodedLength,
                                  std::size_t padding,
                                  std::size_t &decodedLength);

} // namespace ImageShapePngData
