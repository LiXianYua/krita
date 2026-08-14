/*
 * KDE. Krita Project.
 *
 * SPDX-FileCopyrightText: 2022 Deif Lou <ginoba@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <KoCanvasResourceProvider.h>
#include <kis_cursor.h>

#include "KisBrushEnclosingProducer.h"

KisBrushEnclosingProducer::KisBrushEnclosingProducer(KoCanvasBase * canvas)
    : KisDynamicDelegateTool<KisToolBasicBrushBase>(canvas, KisToolBasicBrushBase::PAINT, KisCursor::load("tool_freehand_cursor.xpm", 2, 2))
{
    setObjectName("enclosing_tool_brush");

    connect(canvas->resourceManager(), &KoCanvasResourceProvider::canvasResourceChanged,
            this, [this](int key, const QVariant &) {
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
        useCursor(KisCursor::load("cursor-eraser.xpm", 2, 2));
    } else {
        KisDynamicDelegateTool::resetCursorStyle();
    }

    overrideCursorIfNotEditable();
}

void KisBrushEnclosingProducer::finishStroke(const QPainterPath &stroke)
{
    if (stroke.isEmpty()) {
        return;
    }
    
    KisPixelSelectionSP enclosingMask = new KisPixelSelection();

    KisPainter painter(enclosingMask);
    painter.setPaintColor(KoColor(Qt::white, enclosingMask->colorSpace()));
    painter.setAntiAliasPolygonFill(false);
    painter.setFillStyle(KisPainter::FillStyleForegroundColor);
    painter.setStrokeStyle(KisPainter::StrokeStyleNone);

    painter.fillPainterPath(stroke);

    Q_EMIT enclosingMaskProduced(enclosingMask);
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
