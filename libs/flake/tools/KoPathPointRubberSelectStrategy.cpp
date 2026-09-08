/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006 Jan Hambrecht <jaham@gmx.net>
 * SPDX-FileCopyrightText: 2006 Thorsten Zachmann <zachmann@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KoPathPointRubberSelectStrategy.h"
#include "KoShapeRubberSelectStrategy_p.h"

#include "KoCanvasBase.h"
#include "KoPathTool.h"
#include "KoPathToolSelection.h"

KoPathPointRubberSelectStrategy::KoPathPointRubberSelectStrategy(KoPathTool *tool, const PkPointF &clicked)
        : KoShapeRubberSelectStrategy(tool, clicked)
        , m_tool(tool)
{
}

void KoPathPointRubberSelectStrategy::handleMouseMove(const PkPointF &p, Pk::KeyboardModifiers modifiers)
{
    KoPathToolSelection * selection = dynamic_cast<KoPathToolSelection*>(m_tool->selection());
    if (selection && !(modifiers & Pk::ShiftModifier)) {
        selection->clear();
    }

    KoShapeRubberSelectStrategy::handleMouseMove(p, modifiers);
}

void KoPathPointRubberSelectStrategy::finishInteraction(Pk::KeyboardModifiers modifiers)
{
    Q_D(KoShapeRubberSelectStrategy);
    KoPathToolSelection * selection = dynamic_cast<KoPathToolSelection*>(m_tool->selection());
    if (!selection) {
        return;
    }

    const PkRectF oldDirtyRect = d->selectedRect().normalized() | m_tool->decorationsRect();
    selection->selectPoints(d->selectedRect(), !(modifiers & Pk::ShiftModifier));
    m_tool->canvas()->updateCanvas(oldDirtyRect |
                                   d->selectedRect().normalized() |
                                   m_tool->decorationsRect());
}

void KoPathPointRubberSelectStrategy::cancelInteraction()
{
    Q_D(KoShapeRubberSelectStrategy);

    m_tool->canvas()->updateCanvas(d->selectedRect().normalized() |
                                   m_tool->decorationsRect());
}
