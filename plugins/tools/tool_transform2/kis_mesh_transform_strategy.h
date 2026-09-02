/*
 *  SPDX-FileCopyrightText: 2020 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_MESH_TRANSFORM_STRATEGY_H
#define __KIS_MESH_TRANSFORM_STRATEGY_H

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


class KisMeshTransformStrategy : public KisSimplifiedActionPolicyStrategy
{
public:
    KisMeshTransformStrategy(const KisCoordinatesConverter *converter,
                             KoSnapGuide *snapGuide,
                             ToolTransformArgs &currentArgs,
                             TransformTransactionProperties &transaction);
    ~KisMeshTransformStrategy() override;


    void setTransformFunction(const PkPointF &mousePos, bool perspectiveModifierActive, bool shiftModifierActive, bool altModifierActive) override;
    PkPointF handleSnapPoint(const PkPointF &imagePos) override;
    bool shiftModifierIsUsed() const override;

    void paint(TransformToolPainter &gc) override;
    TransformCursorDescriptor getCurrentCursor() const override;
    void externalConfigChanged() override;

    bool beginPrimaryAction(const PkPointF &pt) override;
    void continuePrimaryAction(const PkPointF &pt, bool shiftModifierActive, bool altModifierActive) override;
    bool endPrimaryAction() override;

    using KisSimplifiedActionPolicyStrategy::beginPrimaryAction;
    using KisSimplifiedActionPolicyStrategy::continuePrimaryAction;
    using KisSimplifiedActionPolicyStrategy::endPrimaryAction;

    bool acceptsClicks() const override;
private:
    bool splitHoveredSegment(const PkPointF &pt);
    bool shouldDeleteNode(qreal distance, qreal param);
    void verifyExpectedMeshSize();

private:
signals:
    void requestCanvasUpdate();
    void requestImageRecalculation();

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif /* __KIS_MESH_TRANSFORM_STRATEGY_H */
