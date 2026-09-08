/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006 Jan Hambrecht <jaham@gmx.net>
 * SPDX-FileCopyrightText: 2006 Thorsten Zachmann <zachmann@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KOPATHPOINTMOVESTRATEGY_H
#define KOPATHPOINTMOVESTRATEGY_H

#include <PkPoint.h>
#include <PkScopedPointer.h>
#include "KoInteractionStrategy.h"

#include <memory>

class KoPathTool;

/**
 * @brief Strategy for moving points of a path shape.
 */
class KoPathPointMoveStrategy : public KoInteractionStrategy
{
public:
    KoPathPointMoveStrategy(KoPathTool *tool, const PkPointF &mousePosition, const PkPointF &pointPosition);
    ~KoPathPointMoveStrategy() override;
    void handleMouseMove(const PkPointF &mouseLocation, Pk::KeyboardModifiers modifiers) override;
    void finishInteraction(Pk::KeyboardModifiers modifiers) override;
    KUndo2Command *createCommand() override;

private:
    PkPointF m_startMousePosition;
    PkPointF m_startPointPosition;
    /// the accumulated point move amount
    PkPointF m_move;
    /// pointer to the path tool
    KoPathTool *m_tool;
    std::unique_ptr<KUndo2Command> m_intermediateCommand;
};

#endif /* KOPATHPOINTMOVESTRATEGY_H */
