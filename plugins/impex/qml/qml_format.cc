/*
 * SPDX-FileCopyrightText: 2026 S-09-c
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "qml_format.h"

#include <PkMemoryStream.h>
#include <PkTextStream.h>

#include <iomanip>
#include <locale>
#include <sstream>

namespace
{

PkString formatNumber(double value)
{
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(6) << std::defaultfloat << value;
    return PkString(stream.str().c_str());
}

bool writeAll(PkStream *device, const char *data, PkStream::pk_int64 size)
{
    PkStream::pk_int64 written = 0;
    while (written < size) {
        const PkStream::pk_int64 chunk = device->write(data + written, size - written);
        if (chunk <= 0) {
            return false;
        }
        written += chunk;
    }
    return true;
}

void writeSetting(PkTextStream &stream,
                  int indentation,
                  const PkString &name,
                  const PkString &value)
{
    for (int i = 0; i < indentation; ++i) {
        stream << "    ";
    }
    stream << name << ": " << value << "\n";
}

} // namespace

bool writeQmlDocument(PkStream *device,
                      int width,
                      int height,
                      const std::vector<QmlLayerRecord> &layers)
{
    if (!device) {
        return false;
    }

    PkMemoryStream rendered;
    if (!rendered.open(PkStream::WriteOnly)) {
        return false;
    }

    {
        PkTextStream stream(&rendered);
        stream << "import QtQuick 1.1\n\n";
        stream << "Rectangle {\n";
        writeSetting(stream, 1, "width", PkString("%1").arg(width));
        writeSetting(stream, 1, "height", PkString("%1").arg(height));
        stream << "\n";

        for (const QmlLayerRecord &layer : layers) {
            stream << "    Image {\n";
            writeSetting(stream, 2, "id", layer.id);
            writeSetting(stream, 2, "x", PkString("%1").arg(layer.bounds.x()));
            writeSetting(stream, 2, "y", PkString("%1").arg(layer.bounds.y()));
            writeSetting(stream, 2, "width", PkString("%1").arg(layer.bounds.width()));
            writeSetting(stream, 2, "height", PkString("%1").arg(layer.bounds.height()));
            writeSetting(stream, 2, "source", PkString("\"") + layer.source + "\"");
            writeSetting(stream, 2, "opacity", formatNumber(layer.opacity));
            stream << "    }\n";
        }
        stream << "}\n";
        stream.flush();
    }

    bool openedHere = false;
    if (!device->isOpen()) {
        openedHere = device->open(PkStream::WriteOnly);
        if (!openedHere) {
            return false;
        }
    }

    const bool ok = writeAll(device, rendered.data(), rendered.size());
    if (openedHere) {
        device->close();
    }
    return ok;
}
