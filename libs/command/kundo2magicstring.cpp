/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *  SPDX-FileCopyrightText: 2014 Alexander Potashev <aspotashev@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "kundo2magicstring.h"


KUndo2MagicString::KUndo2MagicString()
{
}

KUndo2MagicString::KUndo2MagicString(const PkString &text)
    : m_text(text)
{
}

PkString KUndo2MagicString::toString() const
{
    // PkString 无 indexOf(char)；split 复刻 indexOf('\n')>0 语义。
    // 首段非空且存在第二段 ⇔ 第一个 '\n' 在位置 >0（前导换行不算分隔符）。
    const auto parts = m_text.split(u'\n');
    return (parts.size() > 1 && !parts[0].isEmpty()) ? parts[0] : m_text;
}

PkString KUndo2MagicString::toSecondaryString() const
{
    const auto parts = m_text.split(u'\n');
    if (parts.size() > 1 && !parts[0].isEmpty()) {
        PkString result;
        for (std::size_t i = 1; i < parts.size(); ++i) {
            if (i > 1) result.append(PkString("\n"));
            result.append(parts[i]);
        }
        return result;
    }
    return m_text;
}

bool KUndo2MagicString::isEmpty() const
{
    return m_text.isEmpty();
}

bool KUndo2MagicString::operator==(const KUndo2MagicString &rhs) const
{
    return m_text == rhs.m_text;
}
