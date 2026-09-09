/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KoSvgTextNewlineNormalizer.h"

PkString normalizeSvgTextNewlines(const PkString &text)
{
    PkString normalized = text;
    normalized.replace(PkString("\r\n"), PkString("\n"));
    normalized.replace(u'\r', u'\n');
    return normalized;
}
