/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MYPAINTBRUSHUTILS_H
#define MYPAINTBRUSHUTILS_H

#include <PkAuxTypes.h>
#include <PkString.h>

#include <libmypaint/mypaint-brush.h>

#include <vector>

class KisPropertiesConfiguration;

namespace MyPaintBrushUtils
{

class ParseBuffer
{
public:
    explicit ParseBuffer(const PkByteArray &rawBytes);

    const char *data() const;
    int size() const;

private:
    std::vector<char> m_bytes;
};

bool parseBrush(MyPaintBrush *brush, const PkByteArray &rawBytes);

const PkString &preserveSlowTrackingKey();
void applySlowTrackingPolicy(MyPaintBrush *brush,
                             const KisPropertiesConfiguration *settings);

} // namespace MyPaintBrushUtils

#endif // MYPAINTBRUSHUTILS_H
