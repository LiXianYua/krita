/*
 * SPDX-FileCopyrightText: 2026 S-09-c
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include <PkString.h>
#include <PkXmlNode.h>

#include <cstddef>

template<typename Writer>
bool oraWriteAll(Writer writer, const char *data, std::size_t size)
{
    std::size_t written = 0;
    while (written < size) {
        const long long count = writer(data + written,
                                       static_cast<long long>(size - written));
        if (count <= 0 || static_cast<std::size_t>(count) > size - written) {
            return false;
        }
        written += static_cast<std::size_t>(count);
    }
    return true;
}

bool oraLayerPayloadSucceeded(const PkString &path);
void oraAppendStackChild(PkXmlNode parent, const PkXmlNode &child);
