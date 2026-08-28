/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "simplexnoiseseed.h"

#include <KoMD5Generator.h>

#include <cstdint>
#include <string>

namespace {

std::uint8_t hexNibble(char value)
{
    if (value >= '0' && value <= '9') {
        return static_cast<std::uint8_t>(value - '0');
    }
    return static_cast<std::uint8_t>(value - 'a' + 10);
}

}

std::uint64_t simplexNoiseRotateLeft(std::uint64_t input, unsigned int shift)
{
    shift %= 64U;
    if (shift == 0U) {
        return input;
    }
    return (input << shift) | (input >> (64U - shift));
}

std::uint32_t simplexNoiseSeedFromString(const PkString &string)
{
    const std::string bytes = string.PkToUtf8();
    const PkByteArray input(bytes.data(), static_cast<int>(bytes.size()));
    const std::string digestHex = KoMD5Generator::generateHash(input).PkToUtf8();
    if (digestHex.size() != 32U) {
        return 0;
    }

    std::uint32_t hash = 0;
    for (std::size_t index = 0; index < 16U; ++index) {
        const std::uint8_t digestByte = static_cast<std::uint8_t>(
            (hexNibble(digestHex[index * 2U]) << 4U) |
            hexNibble(digestHex[index * 2U + 1U]));
        // The legacy byte-array operator[] returned plain char. Linux/x86_64
        // uses signed char, so preserve its sign extension before
        // the 64-bit rotate to keep persisted custom seeds pixel-identical.
        const std::int8_t signedByte = static_cast<std::int8_t>(digestByte);
        const std::uint64_t value = static_cast<std::uint64_t>(
            static_cast<std::int64_t>(signedByte));
        hash += static_cast<std::uint32_t>(simplexNoiseRotateLeft(
            value, static_cast<unsigned int>(index % 32U)));
    }
    return hash;
}
