/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_CAGE_TRANSFORM_STRATEGY_H
#define __KIS_CAGE_TRANSFORM_STRATEGY_H

#include <PkObject.h>
#include <PkSignalCompat.h>
#include <PkScopedPointer.h>

#include "kis_warp_transform_strategy.h"

class PkPointF;
class PkPainter;
class KisCoordinatesConverter;
class ToolTransformArgs;
class TransformTransactionProperties;
class PkImage;


class KisCageTransformStrategy : public KisWarpTransformStrategy
{
public:
    KisCageTransformStrategy(const KisCoordinatesConverter *converter, KoSnapGuide *snapGuide,
                             ToolTransformArgs &currentArgs,
                             TransformTransactionProperties &transaction);
    ~KisCageTransformStrategy() override;

protected:
    void drawConnectionLines(PkPainter &gc,
                             const PkVector<PkPointF> &origPoints,
                             const PkVector<PkPointF> &transfPoints,
                             bool isEditingPoints) override;

    PkImage calculateTransformedImage(ToolTransformArgs &currentArgs,
                                     const PkImage &srcImage,
                                     const PkVector<PkPointF> &origPoints,
                                     const PkVector<PkPointF> &transfPoints,
                                     const PkPointF &srcOffset,
                                     PkPointF *dstOffset) override;

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif /* __KIS_CAGE_TRANSFORM_STRATEGY_H */
