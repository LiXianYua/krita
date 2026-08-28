/*
 * SPDX-FileCopyrightText: 2026 S-09-c
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

struct SvgImportPolicy {
    double resolutionPpi;
    bool cancelled;
};

constexpr SvgImportPolicy deterministicSvgImportPolicy()
{
    return {100.0, false};
}
