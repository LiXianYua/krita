/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef MULTIGRID_PATTERN_HELPERS_H
#define MULTIGRID_PATTERN_HELPERS_H

#include <PkList.h>
#include <PkPolygon.h>
#include <PkString.h>

struct KisMultiGridRhomb {
    PkPolygonF shape;
    int parallel1;
    int parallel2;
    int line1;
    int line2;
};

PkString multigridDefaultGradientXml();
PkList<KisMultiGridRhomb> generateMultigridRhombs(int lines,
                                                   int divisions,
                                                   qreal offset);

#endif
