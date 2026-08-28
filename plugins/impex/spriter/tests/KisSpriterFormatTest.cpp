/*
 * SPDX-FileCopyrightText: 2026 S-09-c
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "spriter_format.h"

#include <PkStream.h>
#include <PkXmlDocument.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace
{

class OutputStream final : public PkStream
{
public:
    const std::vector<char> &bytes() const { return m_bytes; }
    pk_int64 size() const override { return static_cast<pk_int64>(m_bytes.size()); }
    bool isSequential() const override { return false; }

protected:
    pk_int64 readData(char *, pk_int64) override { return 0; }

    pk_int64 writeData(const char *data, pk_int64 size) override
    {
        const pk_int64 offset = pos();
        const pk_int64 required = offset + size;
        if (required > static_cast<pk_int64>(m_bytes.size())) {
            m_bytes.resize(static_cast<std::size_t>(required));
        }
        std::memcpy(m_bytes.data() + offset, data, static_cast<std::size_t>(size));
        return size;
    }

private:
    std::vector<char> m_bytes;
};

} // namespace

int main()
{
    PkXmlDocument document;
    PkXmlElement root = document.createElement("spriter_data");
    root.setAttribute("generator", "krita");
    document.appendChild(root);

    PkXmlElement entity = document.createElement("entity");
    entity.setAttribute("id", "0");
    entity.setAttribute("name", "画布");
    root.appendChild(entity);

    OutputStream output;
    if (!output.open(PkStream::WriteOnly)) {
        return 1;
    }
    if (!writeSpriterScml(&output, document)) {
        return 2;
    }

    const std::string expected =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<spriter_data generator=\"krita\">\n"
        "    <entity id=\"0\" name=\"画布\" />\n"
        "</spriter_data>\n";

    const std::string actual(output.bytes().begin(), output.bytes().end());
    if (actual != expected) {
        std::cerr << "expected:\n" << expected << "actual:\n" << actual;
        return 3;
    }
    return 0;
}
