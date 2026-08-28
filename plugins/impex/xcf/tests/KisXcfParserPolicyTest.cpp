/*
 * SPDX-FileCopyrightText: 2026 S-09-c
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

extern "C" {
#include "xcftools.h"
}

int main()
{
    std::ifstream input(XCF_PARSER_FIXTURE, std::ios::binary);
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),
                                    std::istreambuf_iterator<char>());
    if (bytes.empty()) {
        std::cerr << "representative XCF fixture could not be read\n";
        return 1;
    }

    xcf_file = bytes.data();
    xcf_length = bytes.size();
    if (getBasicXcfInfo() != XCF_OK) {
        std::cerr << "production xcftools parser rejected representative XCF\n";
        return 2;
    }
    if (XCF.width == 0 || XCF.height == 0 || XCF.numLayers <= 0) {
        std::cerr << "production xcftools parser returned incomplete image metadata\n";
        return 3;
    }
    return 0;
}
