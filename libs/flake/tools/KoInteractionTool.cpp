/* This file is part of the KDE project

   SPDX-FileCopyrightText: 2006-2007, 2010 Thomas Zander <zander@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <pk/render/PkPainter.h>

#include "KoInteractionTool.h"
#include "KoInteractionTool_p.h"
#include "KoToolBase_p.h"
#include "KoPointerEvent.h"
#include "KoCanvasBase.h"

#include "kis_global.h"
#include "kis_assert.h"


KoInteractionTool::KoInteractionTool(KoCanvasBase *canvas)
    : KoToolBase(*(new KoInteractionToolPrivate(this, canvas)))
{
}

KoInteractionTool::~KoInteractionTool()
{
}

void KoInteractionTool::paint(PkPainter &painter, const KoViewConverter &converter)
{
    auto * const d = d_func();

    if (d->currentStrategy) {
        d->currentStrategy->paint(painter, converter);
    } else {
        Q_FOREACH (KoInteractionStrategyFactorySP factory, d->interactionFactories) {
            // skip the rest of rendering if the factory asks for it
            if (factory->paintOnHover(painter, converter)) break;
        }
    }
}

void KoInteractionTool::mousePressEvent(KoPointerEvent *event)
{
    auto * const d = d_func();
    if (d->currentStrategy) { // possible if the user presses an extra mouse button
        cancelCurrentStrategy();
        return;
    }
    d->currentStrategy = createStrategyBase(event);
    if (d->currentStrategy == 0)
        event->ignore();
}

void KoInteractionTool::mouseMoveEvent(KoPointerEvent *event)
{
    auto * const d = d_func();
    d->lastPoint = event->point;

    if (d->currentStrategy) {
        d->currentStrategy->handleMouseMove(
            d->lastPoint, Pk::KeyboardModifiers(static_cast<int>(event->modifiers())));
    } else {
        Q_FOREACH (KoInteractionStrategyFactorySP factory, d->interactionFactories) {
            // skip the rest of rendering if the factory asks for it
            if (factory->hoverEvent(event)) return;
        }

        event->ignore();
    }
}

void KoInteractionTool::mouseReleaseEvent(KoPointerEvent *event)
{
    auto * const d = d_func();
    if (d->currentStrategy) {
        d->currentStrategy->finishInteraction(
            Pk::KeyboardModifiers(static_cast<int>(event->modifiers())));
        KUndo2Command *command = d->currentStrategy->createCommand();
        if (command)
            d->canvas->addCommand(command);
        delete d->currentStrategy;
        d->currentStrategy = 0;
        repaintDecorations();
    } else
        event->ignore();
}

void KoInteractionTool::pkKeyPressEvent(PkToolKeyEvent *event)
{
    auto * const d = d_func();
    event->ignore();
    if (d->currentStrategy &&
            (event->key() == Pk::Key_Control ||
             event->key() == Pk::Key_Alt || event->key() == Pk::Key_Shift ||
             event->key() == Pk::Key_Meta)) {
        d->currentStrategy->handleMouseMove(
            d->lastPoint, event->modifiers());
        event->accept();
    }
}

void KoInteractionTool::pkKeyReleaseEvent(PkToolKeyEvent *event)
{
    auto * const d = d_func();

    if (!d->currentStrategy) {
        KoToolBase::pkKeyReleaseEvent(event);
        return;
    }

    if (event->key() == Pk::Key_Escape) {
        cancelCurrentStrategy();
        event->accept();
    } else if (event->key() == Pk::Key_Control ||
               event->key() == Pk::Key_Alt || event->key() == Pk::Key_Shift ||
               event->key() == Pk::Key_Meta) {
        d->currentStrategy->handleMouseMove(
            d->lastPoint, event->modifiers());
    }
}

KoInteractionStrategy *KoInteractionTool::currentStrategy()
{
    auto * const d = d_func();
    return d->currentStrategy;
}

void KoInteractionTool::cancelCurrentStrategy()
{
    auto * const d = d_func();
    if (d->currentStrategy) {
        d->currentStrategy->cancelInteraction();
        delete d->currentStrategy;
        d->currentStrategy = 0;
    }
}

KoInteractionStrategy *KoInteractionTool::createStrategyBase(KoPointerEvent *event)
{
    auto * const d = d_func();

    Q_FOREACH (KoInteractionStrategyFactorySP factory, d->interactionFactories) {
        KoInteractionStrategy *strategy = factory->createStrategy(event);
        if (strategy) {
            return strategy;
        }
    }

    return createStrategy(event);
}

void KoInteractionTool::addInteractionFactory(KoInteractionStrategyFactory *factory)
{
    auto * const d = d_func();

    Q_FOREACH (auto f, d->interactionFactories) {
        KIS_SAFE_ASSERT_RECOVER_RETURN(f->id() != factory->id());
    }

    d->interactionFactories.append(PkSharedPointer<KoInteractionStrategyFactory>(factory));
    std::sort(d->interactionFactories.begin(),
          d->interactionFactories.end(),
          KoInteractionStrategyFactory::compareLess);
}

void KoInteractionTool::removeInteractionFactory(const PkString &id)
{
    auto * const d = d_func();
    PkList<KoInteractionStrategyFactorySP>::iterator it =
            d->interactionFactories.begin();

    while (it != d->interactionFactories.end()) {
        if ((*it)->id() == id) {
            it = d->interactionFactories.erase(it);
        } else {
            ++it;
        }
    }
}

bool KoInteractionTool::hasInteractionFactory(const PkString &id)
{
    auto * const d = d_func();

    Q_FOREACH (auto f, d->interactionFactories) {
        if (f->id() == id) {
            return true;
        }
    }

    return false;
}

bool KoInteractionTool::tryUseCustomCursor()
{
    auto * const d = d_func();

    Q_FOREACH (auto f, d->interactionFactories) {
        if (f->tryUseCustomCursor()) {
            return true;
        }
    }

    return false;
}

KoInteractionTool::KoInteractionTool(KoInteractionToolPrivate &dd)
    : KoToolBase(dd)
{
}
