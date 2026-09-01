/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006-2007 Thomas Zander <zander@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef SHAPEROTATESTRATEGY_H
#define SHAPEROTATESTRATEGY_H

#include <KoInteractionStrategy.h>

#include <PkPoint.h>
#include <PkRect.h>
#include <PkTransform.h>
#include <PkList.h>

class KoToolBase;
class KoShape;
class KoSelection;

/**
 * A strategy for the KoInteractionTool.
 * This strategy is invoked when the user starts a rotate of a selection of objects,
 * the strategy will then rotate the objects interactively and provide a command afterwards.
 */
class ShapeRotateStrategy : public KoInteractionStrategy
{
public:
    /**
     * Constructor that starts to rotate the objects.
     * @param tool the parent tool which controls this strategy
     * @param clicked the initial point that the user depressed (in pt).
     */
    ShapeRotateStrategy(KoToolBase *tool, KoSelection *selection, const PkPointF &clicked, Qt::MouseButtons buttons);
    ~ShapeRotateStrategy() override {}

    void handleMouseMove(const PkPointF &mouseLocation, Qt::KeyboardModifiers modifiers) override;
    KUndo2Command *createCommand() override;
    void finishInteraction(Qt::KeyboardModifiers modifiers) override
    {
        (void)modifiers;
    }
    void paint(PkPainter &painter, const KoViewConverter &converter) override;

private:
    void rotateBy(qreal angle);

    PkPointF m_start;
    PkTransform m_rotationMatrix;
    PkList<PkTransform> m_oldTransforms;
    PkPointF m_rotationCenter;
    PkList<KoShape *> m_transformedShapesAndSelection;
};

#endif

