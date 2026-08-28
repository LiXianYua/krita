/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "../simplexnoiseseed.h"

#include <cstdint>
#include <iostream>

int main()
{
    struct Case {
        const char *text;
        std::uint32_t expected;
    };
    const Case cases[] = {
        {"abc", 5984500U},
        {"Disney_noisecolor2", 4293338711U},
    };

    for (const Case &test : cases) {
        const std::uint32_t actual = simplexNoiseSeedFromString(PkString(test.text));
        if (actual != test.expected) {
            std::cerr << test.text << ": expected " << test.expected
                      << ", got " << actual << '\n';
            return 1;
        }
    }

    if (simplexNoiseRotateLeft(0x1234U, 0U) != 0x1234U) {
        std::cerr << "zero-bit rotation changed the input\n";
        return 1;
    }
    return 0;
}
