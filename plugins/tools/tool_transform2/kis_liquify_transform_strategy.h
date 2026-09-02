/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_LIQUIFY_TRANSFORM_STRATEGY_H
#define __KIS_LIQUIFY_TRANSFORM_STRATEGY_H

#include <PkObject.h>
#include <PkSignalCompat.h>
#include <PkScopedPointer.h>

#include "kis_transform_strategy_base.h"

class PkPointF;
class PkPainter;
class KisCoordinatesConverter;
class ToolTransformArgs;
class TransformTransactionProperties;
class KisCanvasToolServices;


class KisLiquifyTransformStrategy : public KisTransformStrategyBase
{
public:
    KisLiquifyTransformStrategy(const KisCoordinatesConverter *converter,
                             ToolTransformArgs &currentArgs,
                             TransformTransactionProperties &transaction,
                             const KoCanvasResourceProvider *manager,
                             KisCanvasToolServices *canvasServices);
    ~KisLiquifyTransformStrategy() override;

    void setTransformFunction(const PkPointF &mousePos, bool perspectiveModifierActive, bool shiftModifierActive);
    void paint(TransformToolPainter &gc) override;
    TransformCursorDescriptor getCurrentCursor() const override;
    PkPainterPath getCursorOutline() const override;

    bool acceptsClicks() const override;

    void externalConfigChanged() override;

    bool beginPrimaryAction(KoPointerEvent *event) override;
    void continuePrimaryAction(KoPointerEvent *event) override;
    bool endPrimaryAction(KoPointerEvent *event) override;
    void hoverActionCommon(KoPointerEvent *event) override;

    void activateAlternateAction(KisTool::AlternateAction action) override;
    void deactivateAlternateAction(KisTool::AlternateAction action) override;

    bool beginAlternateAction(KoPointerEvent *event, KisTool::AlternateAction action) override;
    void continueAlternateAction(KoPointerEvent *event, KisTool::AlternateAction action) override;
    bool endAlternateAction(KoPointerEvent *event, KisTool::AlternateAction action) override;

    void increaseBrushSize(KoCanvasBase *canvas) override;
    void decreaseBrushSize(KoCanvasBase *canvas) override;

signals:
    void requestCanvasUpdate();
    void requestUpdateOptionWidget();
    void requestCursorOutlineUpdate(const PkPointF &imagePoint);
    void requestImageRecalculation();

private:
    void changeBrushSize(KoCanvasBase *canvas, bool increase);

    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif /* __KIS_LIQUIFY_TRANSFORM_STRATEGY_H */
