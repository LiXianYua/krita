/*
 * SPDX-FileCopyrightText: 2026 S-09-c
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ora_save_policy.h"

#include <PkXmlDocument.h>

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <string>

int main()
{
    const std::string utf8 = u8"<stack name=\"画层\"/>";
    std::string sink;
    const bool fullWrite = oraWriteAll(
        [&sink](const char *data, long long size) {
            const long long chunk = std::min<long long>(size, 3);
            sink.append(data, static_cast<std::size_t>(chunk));
            return chunk;
        },
        utf8.data(), utf8.size());
    if (!fullWrite || sink != utf8) {
        std::cerr << "UTF-8 payload was not preserved across partial writes\n";
        return 1;
    }

    int calls = 0;
    const bool shortWrite = oraWriteAll(
        [&calls](const char *, long long size) {
            ++calls;
            return calls == 1 ? std::min<long long>(size, 2) : 0;
        },
        utf8.data(), utf8.size());
    if (shortWrite) {
        std::cerr << "zero-progress short write was reported as success\n";
        return 2;
    }

    if (oraLayerPayloadSucceeded(PkString()) || !oraLayerPayloadSucceeded(PkString("data/layer0.png"))) {
        std::cerr << "layer PNG failure sentinel was not preserved\n";
        return 3;
    }

    PkXmlDocument doc("ora-test");
    PkXmlElement stack = doc.createElement("stack");
    doc.appendChild(stack);
    PkXmlElement bottom = doc.createElement("layer");
    bottom.setAttribute("name", PkString::PkFromUtf8(u8"底层", 6));
    PkXmlElement top = doc.createElement("layer");
    top.setAttribute("name", PkString::PkFromUtf8(u8"顶层", 6));
    oraAppendStackChild(stack, bottom);
    oraAppendStackChild(stack, top);

    const PkXmlElement first = stack.firstChildElement("layer");
    if (first.attribute("name") != PkString::PkFromUtf8(u8"底层", 6) ||
        first.nextSiblingElement("layer").attribute("name") != PkString::PkFromUtf8(u8"顶层", 6)) {
        std::cerr << "ORA stack order was not preserved by production insertion helper\n";
        return 4;
    }
    const std::string xml = doc.toByteArray().PkToUtf8();
    if (xml.find(u8"顶层") == std::string::npos || xml.find(u8"底层") == std::string::npos) {
        std::cerr << "ORA stack XML did not preserve UTF-8 layer names\n";
        return 5;
    }

    return 0;
}
