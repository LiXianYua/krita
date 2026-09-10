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

PkList<KisHostActionSpec> KisToolPaintFactoryBase::createActionsImpl()
{
    PkList<KisHostActionSpec> actions;

    actions << createHostAction(
        "Increase Brush Size", "increase_brush_size", Pk::Key_BracketRight);

    actions << createHostAction(
        "Decrease Brush Size", "decrease_brush_size", Pk::Key_BracketLeft);

    actions << createHostAction(
        "Rotate brush tip clockwise", "rotate_brush_tip_clockwise");

    actions << createHostAction(
        "Rotate brush tip clockwise (precise)", "rotate_brush_tip_clockwise_precise");

    actions << createHostAction(
        "Rotate brush tip counter-clockwise", "rotate_brush_tip_counter_clockwise");

    actions << createHostAction(
        "Rotate brush tip counter-clockwise (precise)",
        "rotate_brush_tip_counter_clockwise_precise");

    return actions;
}
