/*
 * SPDX-FileCopyrightText: 2026 S-09-c
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisAnimatedBrushAnnotation.h"

#include <kis_pipebrush_parasite.h>

#include <string>

int main()
{
    KisPipeBrushParasite parasite;
    parasite.ncells = 3;
    parasite.dim = 2;
    parasite.rank[0] = 3;
    parasite.selection[0] = KisParasite::Incremental;
    parasite.rank[1] = 1;
    parasite.selection[1] = KisParasite::Random;

    const KisAnimatedBrushAnnotation annotation(parasite);
    const std::string expected =
        "3 ncells:3 dim:2 rank0:3 sel0:incremental rank1:1 sel1:random";
    const PkByteArray bytes = annotation.annotation();
    const std::string actual(bytes.constData(), static_cast<std::size_t>(bytes.size()));
    return actual == expected ? 0 : 1;
}
