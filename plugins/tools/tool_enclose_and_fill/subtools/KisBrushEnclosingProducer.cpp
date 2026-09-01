/*
 * KDE. Krita Project.
 *
 * SPDX-FileCopyrightText: 2022 Deif Lou <ginoba@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <KoCanvasResourceProvider.h>

#include "KisBrushEnclosingProducer.h"

KisBrushEnclosingProducer::KisBrushEnclosingProducer(KoCanvasBase * canvas)
    : KisDynamicDelegateTool<KisToolBasicBrushBase>(canvas, KisToolBasicBrushBase::PAINT)
{
    setObjectName("enclosing_tool_brush");

    connect(canvas->resourceManager(), &KoCanvasResourceProvider::canvasResourceChanged,
            this, [this](int key, const PkVariant &) {
                if (key == KoCanvasResource::CurrentEffectiveCompositeOp) {
                    resetCursorStyle();
                }
            });
}

KisBrushEnclosingProducer::~KisBrushEnclosingProducer()
{}

void  KisBrushEnclosingProducer::resetCursorStyle()
{
    if (isEraser()) {
        useCursor(Qt::ArrowCursor);
    } else {
        KisDynamicDelegateTool::resetCursorStyle();
    }

    overrideCursorIfNotEditable();
}

void KisBrushEnclosingProducer::enclosingMaskProduced(KisPixelSelectionSP enclosingMask)
{
    PkObject::activateSignal<KisPixelSelectionSP>(
        this,
        PkMemberFnKey::from(&KisBrushEnclosingProducer::enclosingMaskProduced),
        enclosingMask);
}

void KisBrushEnclosingProducer::finishStroke(const PkPainterPath &stroke)
{
    if (stroke.isEmpty()) {
        return;
    }
    
    KisPixelSelectionSP enclosingMask(new KisPixelSelection());

    KisPainter painter(enclosingMask);
    painter.setPaintColor(KoColor(Qt::white, enclosingMask->colorSpace()));
    painter.setAntiAliasPolygonFill(false);
    painter.setFillStyle(KisPainter::FillStyleForegroundColor);
    painter.setStrokeStyle(KisPainter::StrokeStyleNone);

    painter.fillPainterPath(stroke);

    enclosingMaskProduced(enclosingMask);
}

bool KisBrushEnclosingProducer::hasUserInteractionRunning() const
{
    return m_hasUserInteractionRunning;
}

void KisBrushEnclosingProducer::beginShape()
{
    m_hasUserInteractionRunning = true;
}

void KisBrushEnclosingProducer::endShape()
{
    m_hasUserInteractionRunning = false;
}
