/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisHandleStyle.h"
#include "kis_painting_tweaks.h"
#include <PkScopedPointer.h>

namespace {
void initDashedStyle(const PkColor &baseColor, const PkColor &handleFill, KisHandleStyle *style) {
    PkPen ants;
    PkPen outline;
    KisPaintingTweaks::initAntsPen(&ants, &outline);

    ants.setColor(baseColor);

    style->lineIterations << KisHandleStyle::IterationStyle(outline, Qt::NoBrush);
    style->lineIterations << KisHandleStyle::IterationStyle(ants, Qt::NoBrush);

    PkPen handlePen(baseColor);
    handlePen.setWidth(2);
    handlePen.setCosmetic(true);
    handlePen.setJoinStyle(Qt::RoundJoin);

    style->handleIterations << KisHandleStyle::IterationStyle(handlePen, handleFill);
}

static const PkColor primaryColor(0, 0, 90, 180);
static const PkColor secondaryColor(0, 0, 255, 127);
static const PkColor gradientFillColor(255, 197, 39);
static const PkColor highlightColor(255, 100, 100);
static const PkColor highlightOutlineColor(155, 0, 0);
static const PkColor selectionColor(164, 227, 243);

}

KisHandleStyle &KisHandleStyle::inheritStyle()
{
    static PkScopedPointer<KisHandleStyle> style;

    if (!style) {
        style.reset(new KisHandleStyle());
        style->lineIterations << KisHandleStyle::IterationStyle();
        style->handleIterations << KisHandleStyle::IterationStyle();
    }

    return *style;
}

KisHandleStyle &KisHandleStyle::primarySelection()
{
    static PkScopedPointer<KisHandleStyle> style;

    if (!style) {
        style.reset(new KisHandleStyle());
        initDashedStyle(primaryColor, Qt::white, style.data());
    }

    return *style;
}

KisHandleStyle &KisHandleStyle::secondarySelection()
{
    static PkScopedPointer<KisHandleStyle> style;

    if (!style) {
        style.reset(new KisHandleStyle());
        initDashedStyle(secondaryColor, Qt::white, style.data());
    }

    return *style;
}

KisHandleStyle &KisHandleStyle::gradientHandles()
{
    static PkScopedPointer<KisHandleStyle> style;

    if (!style) {
        style.reset(new KisHandleStyle());
        initDashedStyle(primaryColor, gradientFillColor, style.data());
    }

    return *style;
}

KisHandleStyle &KisHandleStyle::gradientArrows()
{
    return primarySelection();
}

KisHandleStyle &KisHandleStyle::highlightedPrimaryHandles()
{
    static PkScopedPointer<KisHandleStyle> style;

    if (!style) {
        style.reset(new KisHandleStyle());
        initDashedStyle(highlightOutlineColor, highlightColor, style.data());
    }

    return *style;
}

KisHandleStyle &KisHandleStyle::highlightedPrimaryHandlesWithSolidOutline()
{
    static PkScopedPointer<KisHandleStyle> style;

    if (!style) {
        style.reset(new KisHandleStyle());
        PkPen h = PkPen(highlightOutlineColor, 2);
        h.setCosmetic(true);
        style->handleIterations << KisHandleStyle::IterationStyle(h, highlightColor);
        PkPen l = PkPen(highlightOutlineColor, 1);
        l.setCosmetic(true);
        l.setJoinStyle(Qt::RoundJoin);
        style->lineIterations << KisHandleStyle::IterationStyle(l, Qt::NoBrush);
    }

    return *style;
}

KisHandleStyle &KisHandleStyle::partiallyHighlightedPrimaryHandles()
{
    static PkScopedPointer<KisHandleStyle> style;

    if (!style) {
        style.reset(new KisHandleStyle());
        initDashedStyle(highlightOutlineColor, selectionColor, style.data());
    }

    return *style;
}

KisHandleStyle &KisHandleStyle::selectedPrimaryHandles()
{
    static PkScopedPointer<KisHandleStyle> style;

    if (!style) {
        style.reset(new KisHandleStyle());
        initDashedStyle(primaryColor, selectionColor, style.data());
    }

    return *style;
}

