/*
 *  SPDX-FileCopyrightText: 2025 Agata Cacko
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef REMOVEGUTTERSTRATEGY_H
#define REMOVEGUTTERSTRATEGY_H

#include <PkScopedPointer.h>
#include <PkRect.h>
#include <PkPainter.h>

#include <KoInteractionStrategy.h>
#include <KoShape.h>

class KoSelection;


class RemoveGutterStrategy : public KoInteractionStrategy
{
public:
    RemoveGutterStrategy(KoToolBase *tool, KoSelection *selection, const PkList<KoShape*> &shapes, PkPointF startPoint);
    ~RemoveGutterStrategy() override;

    KUndo2Command *createCommand() override;

    void handleMouseMove(const PkPointF &mouseLocation, Qt::KeyboardModifiers modifiers) override;
    void finishInteraction(Qt::KeyboardModifiers modifiers) override;
    void paint(PkPainter &painter, const KoViewConverter &converter) override;

private:
    PkPointF m_startPoint = PkPointF();
    PkPointF m_endPoint = PkPointF();
    PkRectF m_previousLineDirtyRect = PkRectF();

    PkList<KoShape *> m_allShapes;
    PkList<KoShape *> m_selectedShapes;

};

#endif // REMOVEGUTTERSTRATEGY_H
