/*
 *  SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include "KisToolPaintFactoryBase.h"

#include <QAction>

#include <klocalizedstring.h>

KisToolPaintFactoryBase::KisToolPaintFactoryBase(const QString &id)
    : KoToolFactoryBase(id)
{
}

KisToolPaintFactoryBase::~KisToolPaintFactoryBase()
{
}

QList<QAction *> KisToolPaintFactoryBase::createActionsImpl()
{
    QList<QAction *> actions;

    QAction *increaseBrushSize = new QAction(i18n("Increase Brush Size"), this);
    increaseBrushSize->setObjectName("increase_brush_size");
    increaseBrushSize->setShortcut(Qt::Key_BracketRight);

    actions << increaseBrushSize;

    QAction *decreaseBrushSize = new QAction(i18n("Decrease Brush Size"), this);
    decreaseBrushSize->setShortcut(Qt::Key_BracketLeft);
    decreaseBrushSize->setObjectName("decrease_brush_size");

    actions << decreaseBrushSize;

    QAction *rotateBrushTipClockwise = new QAction(i18n("Rotate brush tip clockwise"), this);
    rotateBrushTipClockwise->setObjectName("rotate_brush_tip_clockwise");

    actions << rotateBrushTipClockwise;

    QAction *rotateBrushTipClockwisePrecise = new QAction(i18n("Rotate brush tip clockwise (precise)"), this);
    rotateBrushTipClockwisePrecise->setObjectName("rotate_brush_tip_clockwise_precise");

    actions << rotateBrushTipClockwisePrecise;

    QAction *rotateBrushTipCounterClockwise = new QAction(i18n("Rotate brush tip counter-clockwise"), this);
    rotateBrushTipCounterClockwise->setObjectName("rotate_brush_tip_counter_clockwise");

    actions << rotateBrushTipCounterClockwise;

    QAction *rotateBrushTipCounterClockwisePrecise = new QAction(i18n("Rotate brush tip counter-clockwise (precise)"), this);
    rotateBrushTipCounterClockwisePrecise->setObjectName("rotate_brush_tip_counter_clockwise_precise");

    actions << rotateBrushTipCounterClockwisePrecise;

    return actions;
}
