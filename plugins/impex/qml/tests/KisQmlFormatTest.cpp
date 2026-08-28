/*
 * SPDX-FileCopyrightText: 2026 S-09-c
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qml_format.h"

#include <PkMemoryStream.h>

#include <string>
#include <vector>

int main()
{
    PkMemoryStream output;
    if (!output.open(PkStream::WriteOnly)) {
        return 1;
    }

    const std::vector<QmlLayerRecord> layers {
        {PkString("foreground_层"), PkRect(2, 3, 5, 7),
         PkString("scene_images/foreground_层.png"), 0.5}
    };
    if (!writeQmlDocument(&output, 11, 13, layers)) {
        return 2;
    }

    const std::string expected =
        "import QtQuick 1.1\n\n"
        "Rectangle {\n"
        "    width: 11\n"
        "    height: 13\n\n"
        "    Image {\n"
        "        id: foreground_层\n"
        "        x: 2\n"
        "        y: 3\n"
        "        width: 5\n"
        "        height: 7\n"
        "        source: \"scene_images/foreground_层.png\"\n"
        "        opacity: 0.5\n"
        "    }\n"
        "}\n";
    const std::string actual(output.data(), static_cast<std::size_t>(output.size()));
    return actual == expected ? 0 : 3;
}
