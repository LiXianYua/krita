/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_FREE_TRANSFORM_STRATEGY_H
#define __KIS_FREE_TRANSFORM_STRATEGY_H

#include <PkObject.h>
#include <PkSignalCompat.h>
#include <PkScopedPointer.h>

#include "kis_simplified_action_policy_strategy.h"

class PkPointF;
class PkPainter;
class KisCoordinatesConverter;
class ToolTransformArgs;
class TransformTransactionProperties;

class KisFreeTransformStrategy : public KisSimplifiedActionPolicyStrategy
{
public:
    KisFreeTransformStrategy(const KisCoordinatesConverter *converter,
                             KoSnapGuide *snapGuide,
                             ToolTransformArgs &currentArgs,
                             TransformTransactionProperties &transaction);
    ~KisFreeTransformStrategy() override;

    void setTransformFunction(const PkPointF &mousePos, bool perspectiveModifierActive, bool shiftModifierActive, bool altModifierActive) override;
    bool shiftModifierIsUsed() const override;

    void paint(TransformToolPainter &gc) override;
    TransformCursorDescriptor getCurrentCursor() const override;

    void externalConfigChanged() override;

    using KisTransformStrategyBase::beginPrimaryAction;
    using KisTransformStrategyBase::continuePrimaryAction;
    using KisTransformStrategyBase::endPrimaryAction;

    bool beginPrimaryAction(const PkPointF &pt) override;
    void continuePrimaryAction(const PkPointF &pt, bool shiftModifierActive, bool altModifierActive) override;
    bool endPrimaryAction() override;

signals:
    void requestCanvasUpdate();
    void requestResetRotationCenterButtons();
    void requestShowImageTooBig(bool value);
    void requestImageRecalculation();
    void requestConvexHullCalculation();

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif /* __KIS_FREE_TRANSFORM_STRATEGY_H */
