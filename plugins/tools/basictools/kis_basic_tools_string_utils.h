/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_BASIC_TOOLS_STRING_UTILS_H
#define KIS_BASIC_TOOLS_STRING_UTILS_H

#include <PkString.h>

#include <charconv>
#include <type_traits>

namespace KisBasicToolsString
{

template<typename Integer,
         typename = std::enable_if_t<std::is_integral<Integer>::value>>
inline PkString number(Integer value)
{
    char buffer[32];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    return PkString::PkFromUtf8(buffer, static_cast<int>(result.ptr - buffer));
}

inline PkString numberFixed(double value, int precision)
{
    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value,
                                      std::chars_format::fixed, precision);
    return PkString::PkFromUtf8(buffer, static_cast<int>(result.ptr - buffer));
}

}

#endif
