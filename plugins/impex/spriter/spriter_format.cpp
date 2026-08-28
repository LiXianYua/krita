/*
 * SPDX-FileCopyrightText: 2026 S-09-c
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "spriter_format.h"

#include <PkStream.h>
#include <PkString.h>
#include <PkXmlDocument.h>

#include <string>

namespace
{

bool writeAll(PkStream *device, const std::string &bytes)
{
    PkStream::pk_int64 written = 0;
    const PkStream::pk_int64 size = static_cast<PkStream::pk_int64>(bytes.size());
    while (written < size) {
        const PkStream::pk_int64 chunk = device->write(bytes.data() + written, size - written);
        if (chunk <= 0) {
            return false;
        }
        written += chunk;
    }
    return true;
}

} // namespace

bool writeSpriterScml(PkStream *device, const PkXmlDocument &document)
{
    if (!device) {
        return false;
    }

    bool openedHere = false;
    if (!device->isOpen()) {
        openedHere = device->open(PkStream::WriteOnly);
        if (!openedHere) {
            return false;
        }
    }

    const std::string bytes =
        std::string("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n") +
        document.toString(4).PkToUtf8();
    const bool ok = writeAll(device, bytes);

    if (openedHere) {
        device->close();
    }
    return ok;
}
