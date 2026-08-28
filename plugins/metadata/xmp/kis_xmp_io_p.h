/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#ifndef KIS_XMP_IO_P_H
#define KIS_XMP_IO_P_H

#include <charconv>
#include <cstdint>
#include <limits>
#include <string>

#include <unicode/uchar.h>

namespace KisXmpIOPrivate
{
inline bool isAsciiLetter(char16_t codeUnit)
{
    return (codeUnit >= u'A' && codeUnit <= u'Z')
        || (codeUnit >= u'a' && codeUnit <= u'z');
}

inline bool isUnicodeWordContinuation(UChar32 codePoint)
{
    if (codePoint == 0x005f) {
        return true;
    }
    const std::uint32_t categoryMask = U_MASK(u_charType(codePoint));
    return (categoryMask & (U_GC_L_MASK | U_GC_N_MASK)) != 0;
}

inline bool isStructuredIdentifier(const std::u16string &identifier)
{
    // The original Qt pattern was [A-Za-z]\w+, so every identifier part has
    // an ASCII first code point and at least one Unicode-word continuation.
    if (identifier.size() < 2 || !isAsciiLetter(identifier.front())) {
        return false;
    }
    for (std::size_t i = 1; i < identifier.size();) {
        UChar32 codePoint = identifier[i++];
        if (U16_IS_LEAD(codePoint)) {
            if (i >= identifier.size() || !U16_IS_TRAIL(identifier[i])) {
                return false;
            }
            codePoint = U16_GET_SUPPLEMENTARY(codePoint, identifier[i++]);
        } else if (U16_IS_TRAIL(codePoint)) {
            return false;
        }
        if (!isUnicodeWordContinuation(codePoint)) {
            return false;
        }
    }
    return true;
}

inline bool parsePositiveArrayIndex(const std::u16string &digits, int &arrayIndex)
{
    if (digits.empty()) {
        return false;
    }
    std::string ascii;
    ascii.reserve(digits.size());
    for (const char16_t codeUnit : digits) {
        if (codeUnit < u'0' || codeUnit > u'9') {
            return false;
        }
        ascii.push_back(static_cast<char>(codeUnit));
    }

    std::uint64_t oneBasedIndex = 0;
    const auto conversion = std::from_chars(ascii.data(),
                                            ascii.data() + ascii.size(),
                                            oneBasedIndex);
    if (conversion.ec != std::errc() || conversion.ptr != ascii.data() + ascii.size()
        || oneBasedIndex == 0
        || oneBasedIndex > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        return false;
    }
    arrayIndex = static_cast<int>(oneBasedIndex - 1);
    return true;
}

template<typename Array>
bool ensureArraySlot(Array &array, int arrayIndex)
{
    const int currentSize = array.size();
    if (arrayIndex < 0 || arrayIndex > currentSize) {
        return false;
    }
    if (arrayIndex == currentSize) {
        if (currentSize == std::numeric_limits<int>::max()) {
            return false;
        }
        array.resize(currentSize + 1);
    }
    return true;
}
}

#endif
