/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ShapeGradientEditStrategy.h"
#include "DefaultToolStrategyMath.h"

#include <KoToolBase.h>
#include <KoCanvasBase.h>
#include <KoCanvasResourceProvider.h>
#include <KoShapeManager.h>
#include <KoShape.h>
#include "kis_assert.h"
#include "SelectionDecorator.h"
#include <kundo2command.h>
#include <kis_command_utils.h>
#include <KoSnapGuide.h>
#include <KisSnapPointStrategy.h>

#include "kis_debug.h"


struct ShapeGradientEditStrategy::Private
{
    Private(const PkPointF &_start, KoShape *shape, KoFlake::FillVariant fillVariant)
        : start(_start),
          gradientHandles(fillVariant, shape)
    {
        previous = start;
    }

    PkPointF start;
    PkPointF previous;
    PkPointF initialOffset;
    KoShapeGradientHandles gradientHandles;
    KoShapeGradientHandles::Handle::Type handleType {KoShapeGradientHandles::Handle::Type::None};
    std::unique_ptr<KUndo2Command> intermediateCommand;
};


ShapeGradientEditStrategy::ShapeGradientEditStrategy(KoToolBase *tool,
                                                     KoFlake::FillVariant fillVariant,
                                                     KoShape *shape,
                                                     KoShapeGradientHandles::Handle::Type startHandleType,
                                                     const PkPointF &clicked)
    : KoInteractionStrategy(tool)
    , m_d(new Private(clicked, shape, fillVariant))
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(shape);

    m_d->handleType = startHandleType;

    KoShapeGradientHandles::Handle handle = m_d->gradientHandles.getHandle(m_d->handleType);
    m_d->initialOffset = handle.pos - clicked;

    KisSnapPointStrategy *strategy = new KisSnapPointStrategy();
    for (const KoShapeGradientHandles::Handle &h : m_d->gradientHandles.handles()) {
        strategy->addPoint(h.pos);
    }
    tool->canvas()->snapGuide()->addCustomSnapStrategy(strategy);
}

ShapeGradientEditStrategy::~ShapeGradientEditStrategy()
{
}

void ShapeGradientEditStrategy::handleMouseMove(const PkPointF &mouseLocation, Qt::KeyboardModifiers modifiers)
{
    const PkPointF snappedPosition = tool()->canvas()->snapGuide()->snap(mouseLocation, m_d->initialOffset, modifiers);
    const PkPointF diff = DefaultToolStrategyMath::gradientHandlePosition(
        m_d->previous, snappedPosition - m_d->previous) - m_d->previous;
    m_d->previous = snappedPosition;

    KisCommandUtils::redoAndMergeIntoAccumulatingCommand(
        m_d->gradientHandles.moveGradientHandle(m_d->handleType, diff),
        m_d->intermediateCommand);
}

KUndo2Command *ShapeGradientEditStrategy::createCommand()
{
    return m_d->intermediateCommand ?
        new KisCommandUtils::SkipFirstRedoWrapper(m_d->intermediateCommand.release()) :
        nullptr;
}

void ShapeGradientEditStrategy::finishInteraction(Qt::KeyboardModifiers modifiers)
{
    (void)modifiers;
    tool()->canvas()->snapGuide()->reset();
}

void ShapeGradientEditStrategy::paint(PkPainter &painter, const KoViewConverter &converter)
{
    (void)painter;
    (void)converter;
}
