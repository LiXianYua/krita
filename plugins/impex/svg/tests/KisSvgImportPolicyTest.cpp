/*
 * SPDX-FileCopyrightText: 2026 S-09-c
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "svg_import_policy.h"

#include <iostream>

int main()
{
    const SvgImportPolicy policy = deterministicSvgImportPolicy();
    if (policy.resolutionPpi != 100.0) {
        std::cerr << "SVG import default PPI changed\n";
        return 1;
    }
    if (policy.cancelled) {
        std::cerr << "headless deterministic policy must not synthesize cancellation\n";
        return 2;
    }
    return 0;
}
