/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <PkImage.h>
#include <string>

// Decode linked reference-image pixels and normalize a valid embedded ICC
// profile to sRGB before metadata is discarded by the native image value.
PkImage loadPkReferenceImage(const std::string &path);
