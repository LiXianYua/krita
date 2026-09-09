/*
 *  SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include <PkPainterPath.h>
#include <PkPen.h>
#include <PkFlakeBridge.h>
#include <pk/container/PkList.h>
#include "KisToolPaintFactoryBase.h"

KisToolPaintFactoryBase::KisToolPaintFactoryBase(const PkString &id)
    : KoToolFactoryBase(id)
{
}

KisToolPaintFactoryBase::~KisToolPaintFactoryBase()
{
}

PkList<QAction *> KisToolPaintFactoryBase::createActionsImpl()
{
    PkList<QAction *> actions;

    QAction *increaseBrushSize = createHostAction(
        "Increase Brush Size", "increase_brush_size", Pk::Key_BracketRight);

    actions << increaseBrushSize;

    QAction *decreaseBrushSize = createHostAction(
        "Decrease Brush Size", "decrease_brush_size", Pk::Key_BracketLeft);

    actions << decreaseBrushSize;

    QAction *rotateBrushTipClockwise = createHostAction(
        "Rotate brush tip clockwise", "rotate_brush_tip_clockwise");

    actions << rotateBrushTipClockwise;

    QAction *rotateBrushTipClockwisePrecise = createHostAction(
        "Rotate brush tip clockwise (precise)", "rotate_brush_tip_clockwise_precise");

    actions << rotateBrushTipClockwisePrecise;

    QAction *rotateBrushTipCounterClockwise = createHostAction(
        "Rotate brush tip counter-clockwise", "rotate_brush_tip_counter_clockwise");

    actions << rotateBrushTipCounterClockwise;

    QAction *rotateBrushTipCounterClockwisePrecise = createHostAction(
        "Rotate brush tip counter-clockwise (precise)",
        "rotate_brush_tip_counter_clockwise_precise");

    actions << rotateBrushTipCounterClockwisePrecise;

    return actions;
}
