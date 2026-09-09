/* This file is part of the KDE project
   SPDX-FileCopyrightText: 2006 Thorsten Zachmann <zachmann@kde.org>
   SPDX-FileCopyrightText: 2007 Thomas Zander <zander@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/
#include "KoParameterChangeStrategy.h"
#include "KoParameterChangeStrategy_p.h"
#include "KoParameterShape.h"
#include "commands/KoParameterHandleMoveCommand.h"

#include <KoCanvasBase.h>
#include "KoSnapGuide.h"


KoParameterChangeStrategy::KoParameterChangeStrategy(KoToolBase *tool, KoParameterShape *parameterShape, int handleId)
    : KoInteractionStrategy(*(new KoParameterChangeStrategyPrivate(tool, parameterShape, handleId)))
{
    auto * const d = d_func();
    d->tool->canvas()->snapGuide()->setIgnoredShapes({parameterShape});
}

KoParameterChangeStrategy::KoParameterChangeStrategy(KoParameterChangeStrategyPrivate& dd)
: KoInteractionStrategy(dd)
{

}

KoParameterChangeStrategy::~KoParameterChangeStrategy()
{
}

void KoParameterChangeStrategy::handleMouseMove(const PkPointF &mouseLocation, Pk::KeyboardModifiers modifiers)
{
    auto * const d = d_func();

    const PkPointF snappedPosition = d->tool->canvas()->snapGuide()->snap(
        mouseLocation, modifiers);

    d->parameterShape->moveHandle(
        d->handleId, snappedPosition, modifiers);
    d->lastModifierUsed = modifiers;
    d->releasePoint = snappedPosition;
}

KUndo2Command* KoParameterChangeStrategy::createCommand()
{
    auto * const d = d_func();

    d->tool->canvas()->snapGuide()->reset();

    KoParameterHandleMoveCommand *cmd = 0;
    // check if handle position changed
    if (d->startPoint != PkPointF(0, 0) && d->startPoint != d->releasePoint) {
        cmd = new KoParameterHandleMoveCommand(
            d->parameterShape,
            d->handleId,
            d->startPoint,
            d->releasePoint,
            d->lastModifierUsed);
    }
    return cmd;
}

void KoParameterChangeStrategy::finishInteraction(Pk::KeyboardModifiers /*modifiers*/)
{
}
