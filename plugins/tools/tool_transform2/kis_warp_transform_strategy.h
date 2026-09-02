/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_WARP_TRANSFORM_STRATEGY_H
#define __KIS_WARP_TRANSFORM_STRATEGY_H

#include <PkObject.h>
#include <PkSignalCompat.h>
#include <PkScopedPointer.h>

#include "kis_simplified_action_policy_strategy.h"

class PkPointF;
class PkPainter;
class KisCoordinatesConverter;
class ToolTransformArgs;
class TransformTransactionProperties;
class PkImage;

enum TransformType {
    WARP_TRANSFORM,
    CAGE_TRANSFORM,
    MESH_TRANSFORM
};

class KisWarpTransformStrategy : public KisSimplifiedActionPolicyStrategy
{
public:
    KisWarpTransformStrategy(const KisCoordinatesConverter *converter,
                             KoSnapGuide *snapGuide,
                             ToolTransformArgs &currentArgs,
                             TransformTransactionProperties &transaction);
    ~KisWarpTransformStrategy() override;

    void setTransformFunction(const PkPointF &mousePos, bool perspectiveModifierActive, bool shiftModifierActive, bool altModifierActive) override;
    void setTransformType(TransformType type);

    void paint(TransformToolPainter &gc) override;
    TransformCursorDescriptor getCurrentCursor() const override;

    void externalConfigChanged() override;

    using KisTransformStrategyBase::beginPrimaryAction;
    using KisTransformStrategyBase::continuePrimaryAction;
    using KisTransformStrategyBase::endPrimaryAction;

    bool beginPrimaryAction(const PkPointF &pt) override;
    void continuePrimaryAction(const PkPointF &pt, bool shiftModifierActive, bool altModifierActive) override;
    bool endPrimaryAction() override;

    bool acceptsClicks() const override;

private:
signals:
    void requestCanvasUpdate();
    void requestImageRecalculation();

protected:
    // default is true
    void setClipOriginalPointsPosition(bool value);

    // default is false
    void setCloseOnStartPointClick(bool value);

    void overrideDrawingItems(bool drawConnectionLines,
                              bool drawOrigPoints,
                              bool drawTransfPoints);

    virtual void drawConnectionLines(PkPainter &gc,
                                     const PkVector<PkPointF> &origPoints,
                                     const PkVector<PkPointF> &transfPoints,
                                     bool isEditingPoints);

    virtual PkImage calculateTransformedImage(ToolTransformArgs &currentArgs,
                                             const PkImage &srcImage,
                                             const PkVector<PkPointF> &origPoints,
                                             const PkVector<PkPointF> &transfPoints,
                                             const PkPointF &srcOffset,
                                             PkPointF *dstOffset);
private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif /* __KIS_WARP_TRANSFORM_STRATEGY_H */
