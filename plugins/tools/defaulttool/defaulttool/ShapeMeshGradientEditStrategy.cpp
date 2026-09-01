/*
 *  SPDX-FileCopyrightText: 2020 Sharaf Zaman <sharafzaz121@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ShapeMeshGradientEditStrategy.h"
#include "DefaultToolStrategyMath.h"

#include <KoToolBase.h>
#include <KoCanvasBase.h>
#include <kundo2command.h>
#include <kis_command_utils.h>

struct ShapeMeshGradientEditStrategy::Private {
    Private(const PkPointF& start, KoShape *shape, KoFlake::FillVariant fillVariant)
        : start(start)
        , handles(fillVariant, shape)
    {
    }

    // TODO: for snapping..
    PkPointF start;
    KoShapeMeshGradientHandles::Handle startHandle;
    KoShapeMeshGradientHandles handles;
    std::unique_ptr<KUndo2Command> intermediateCommand;
};

ShapeMeshGradientEditStrategy::ShapeMeshGradientEditStrategy(KoToolBase *tool,
                                                             KoFlake::FillVariant fillVariant,
                                                             KoShape *shape,
                                                             KoShapeMeshGradientHandles::Handle startHandle,
                                                             const PkPointF &clicked)
    : KoInteractionStrategy(tool)
    , m_d(new Private(clicked, shape, fillVariant))
{
    m_d->startHandle = startHandle;
}

ShapeMeshGradientEditStrategy::~ShapeMeshGradientEditStrategy()
{
}

void ShapeMeshGradientEditStrategy::handleMouseMove(const PkPointF &mouseLocation,
                                                    Qt::KeyboardModifiers modifiers)
{
    (void)modifiers;


    KisCommandUtils::redoAndMergeIntoAccumulatingCommand(
        m_d->handles.moveGradientHandle(
            m_d->startHandle,
            DefaultToolStrategyMath::meshHandlePosition(
                m_d->startHandle.pos, mouseLocation - m_d->startHandle.pos)),
        m_d->intermediateCommand);
}

KUndo2Command* ShapeMeshGradientEditStrategy::createCommand()
{
    return m_d->intermediateCommand ?
        new KisCommandUtils::SkipFirstRedoWrapper(m_d->intermediateCommand.release()) :
        nullptr;
}

void ShapeMeshGradientEditStrategy::finishInteraction(Qt::KeyboardModifiers modifiers)
{
    (void)modifiers;
}
