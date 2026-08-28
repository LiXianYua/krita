/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SIMPLEXNOISESEED_H
#define SIMPLEXNOISESEED_H

#include <PkString.h>

#include <cstdint>

std::uint64_t simplexNoiseRotateLeft(std::uint64_t input, unsigned int shift);
std::uint32_t simplexNoiseSeedFromString(const PkString &string);

#endif
