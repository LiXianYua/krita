/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#pragma once

#include <PkImage.h>
#include <PkString.h>

namespace KisResourceThumbnailCodec
{

PkImage loadPng(const PkString &path);
bool savePng(const PkString &path, const PkImage &image);

}
