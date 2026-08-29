/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "MyPaintBrushUtils.h"

#include <kis_properties_configuration.h>

#include <cstring>

namespace MyPaintBrushUtils
{

ParseBuffer::ParseBuffer(const PkByteArray &rawBytes)
    : m_bytes(static_cast<std::size_t>(rawBytes.size()) + 1, '\0')
{
    if (!rawBytes.isEmpty()) {
        std::memcpy(m_bytes.data(), rawBytes.constData(),
                    static_cast<std::size_t>(rawBytes.size()));
    }
}

const char *ParseBuffer::data() const
{
    return m_bytes.data();
}

int ParseBuffer::size() const
{
    return static_cast<int>(m_bytes.size());
}

bool parseBrush(MyPaintBrush *brush, const PkByteArray &rawBytes)
{
    const ParseBuffer parseBuffer(rawBytes);
    return mypaint_brush_from_string(brush, parseBuffer.data());
}

const PkString &preserveSlowTrackingKey()
{
    static const PkString key("mypaint/preserve_slow_tracking");
    return key;
}

void applySlowTrackingPolicy(MyPaintBrush *brush,
                             const KisPropertiesConfiguration *settings)
{
    if (!settings || !settings->getBool(preserveSlowTrackingKey(), false)) {
        mypaint_brush_set_base_value(
            brush, MYPAINT_BRUSH_SETTING_SLOW_TRACKING, 0.0f);
    }
}

} // namespace MyPaintBrushUtils
