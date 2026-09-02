/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_transform_strategy_base.h"

#include <PkImage.h>
#include <PkPainterPath.h>
#include <PkTransform.h>
#include "KoPointerEvent.h"


struct KisTransformStrategyBase::Private
{
    PkTransform thumbToImageTransform;
    PkImage originalImage;
    int decorationThickness = 1;
};


KisTransformStrategyBase::KisTransformStrategyBase()
    : m_d(new Private())
{
}

KisTransformStrategyBase::~KisTransformStrategyBase()
{
}

PkPainterPath KisTransformStrategyBase::getCursorOutline() const
{
    return PkPainterPath();
}

void KisTransformStrategyBase::activatePrimaryAction()
{
}

void KisTransformStrategyBase::deactivatePrimaryAction()
{
}

void KisTransformStrategyBase::setDecorationThickness(int thickness)
{
    m_d->decorationThickness = qMax(1, thickness);
}

int KisTransformStrategyBase::decorationThickness() const
{
    return m_d->decorationThickness;
}

PkImage KisTransformStrategyBase::originalImage() const
{
    return m_d->originalImage;
}

PkTransform KisTransformStrategyBase::thumbToImageTransform() const
{
    return m_d->thumbToImageTransform;
}

void KisTransformStrategyBase::setThumbnailImage(const PkImage &image, PkTransform thumbToImageTransform)
{
    m_d->originalImage = image;
    m_d->thumbToImageTransform = thumbToImageTransform;
}

bool KisTransformStrategyBase::acceptsClicks() const
{
    return false;
}

void KisTransformStrategyBase::activateAlternateAction(KisTool::AlternateAction action)
{
    (void)action;
}

void KisTransformStrategyBase::deactivateAlternateAction(KisTool::AlternateAction action)
{
    (void)action;
}

bool KisTransformStrategyBase::beginAlternateAction(KoPointerEvent *event, KisTool::AlternateAction action)
{
    (void)event;
    (void)action;
    return false;
}

void KisTransformStrategyBase::continueAlternateAction(KoPointerEvent *event, KisTool::AlternateAction action)
{
    (void)event;
    (void)action;
}

bool KisTransformStrategyBase::endAlternateAction(KoPointerEvent *event, KisTool::AlternateAction action)
{
    (void)event;
    (void)action;
    return false;
}

void KisTransformStrategyBase::increaseBrushSize(KoCanvasBase *canvas)
{
    (void)canvas;
}

void KisTransformStrategyBase::decreaseBrushSize(KoCanvasBase *canvas)
{
    (void)canvas;
}
