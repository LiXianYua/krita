/* This file is part of the KDE project

   SPDX-FileCopyrightText: 2006 Thorsten Zachmann <zachmann@kde.org>
   SPDX-FileCopyrightText: 2006-2007 Thomas Zander <zander@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef SHAPEMOVESTRATEGY_H
#define SHAPEMOVESTRATEGY_H

#include <KoInteractionStrategy.h>

#include <PkPointer.h>
#include <PkPoint.h>
#include <PkList.h>

#include <KoCanvasBase.h>

class KoToolBase;
class KoShape;
class KoSelection;

/**
 * Implements the Move action on an object or selected objects.
 */
class ShapeMoveStrategy : public KoInteractionStrategy
{
public:
    /**
     * Constructor that starts to move the objects.
     * @param tool the parent tool which controls this strategy
     * @param canvas the canvas interface which will supply things like a selection object
     * @param clicked the initial point that the user depressed (in pt).
     */
    ShapeMoveStrategy(KoToolBase *tool, KoSelection *selection, const PkPointF &clicked);
    ~ShapeMoveStrategy() override {}

    void handleMouseMove(const PkPointF &mouseLocation, Qt::KeyboardModifiers modifiers) override;
    KUndo2Command *createCommand() override;
    void finishInteraction(Qt::KeyboardModifiers modifiers) override;
    void paint(PkPainter &painter, const KoViewConverter &converter) override;
private:
    void moveSelection(const PkPointF &diff);
    PkList<PkPointF> m_previousPositions;
    PkList<PkPointF> m_newPositions;
    PkPointF m_start, m_finalMove, m_initialOffset;
    PkList<KoShape *> m_selectedShapes;
    PkPointer<KoCanvasBase> m_canvas;
};

#endif
