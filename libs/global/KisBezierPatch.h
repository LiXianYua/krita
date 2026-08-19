/*
 *  SPDX-FileCopyrightText: 2020 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISBEZIERPATCH_H
#define KISBEZIERPATCH_H

#include "kritaglobal_export.h"

#include <PkRect.h>
#include <array>

class PkDebug;

class KRITAGLOBAL_EXPORT KisBezierPatch
{
public:
    enum ControlPointType {
        TL = 0,
        TL_HC,
        TL_VC,
        TR,
        TR_HC,
        TR_VC,
        BL,
        BL_HC,
        BL_VC,
        BR,
        BR_HC,
        BR_VC
    };

    PkRectF originalRect;
    std::array<PkPointF, 12> points;

    PkRectF dstBoundingRect() const;

    PkRectF srcBoundingRect() const;

    PkPointF localToGlobal(const PkPointF &pt) const;
    PkPointF globalToLocal(const PkPointF &pt) const;

    void sampleRegularGrid(PkSize &gridSize,
                           PkVector<PkPointF> &origPoints,
                           PkVector<PkPointF> &transfPoints,
                           const PkPointF &dstStep) const;

    void sampleRegularGridSVG2(PkSize &gridSize,
                               PkVector<PkPointF> &origPoints,
                               PkVector<PkPointF> &transfPoints,
                               const PkPointF &dstStep) const;
};

KRITAGLOBAL_EXPORT
PkDebug operator<<(PkDebug dbg, const KisBezierPatch &p);

#endif // KISBEZIERPATCH_H
