/*
 * KDE. Krita Project.
 *
 * SPDX-FileCopyrightText: 2022 Deif Lou <ginoba@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <KisCanvasToolServices.h>

#include "KisBrushEnclosingProducer.h"

KisBrushEnclosingProducer::KisBrushEnclosingProducer(KoCanvasBase * canvas)
    : KisDynamicDelegateTool<KisToolBasicBrushBase>(canvas, KisToolBasicBrushBase::PAINT)
{
    setObjectName("enclosing_tool_brush");

    KisCanvasToolServices *services = dynamic_cast<KisCanvasToolServices*>(canvas);
    KIS_ASSERT_RECOVER_RETURN(services);
    PkObject::connect(services->toolSignals(),
                      &KisCanvasToolSignals::effectiveCompositeOpChanged,
                      this, &KisBrushEnclosingProducer::resetCursorStyle);
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
    painter.setPaintColor(KoColor(Pk::white, enclosingMask->colorSpace()));
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
