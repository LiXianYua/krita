/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_CANVAS_CURSOR_TOKEN_H
#define KIS_CANVAS_CURSOR_TOKEN_H

#include <cstdint>

/** Toolkit-neutral identity for a cursor owned by the UI host. */
class KisCanvasCursorToken
{
public:
    constexpr KisCanvasCursorToken() = default;
    explicit constexpr KisCanvasCursorToken(std::uint64_t value)
        : m_value(value)
    {
    }

    constexpr std::uint64_t value() const { return m_value; }
    explicit constexpr operator bool() const { return m_value != 0; }

    friend constexpr bool operator==(KisCanvasCursorToken lhs,
                                     KisCanvasCursorToken rhs)
    {
        return lhs.m_value == rhs.m_value;
    }

    friend constexpr bool operator!=(KisCanvasCursorToken lhs,
                                     KisCanvasCursorToken rhs)
    {
        return !(lhs == rhs);
    }

private:
    std::uint64_t m_value = 0;
};

#endif // KIS_CANVAS_CURSOR_TOKEN_H
