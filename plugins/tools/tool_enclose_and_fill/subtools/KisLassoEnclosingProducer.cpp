/*
 * KDE. Krita Project.
 *
 * SPDX-FileCopyrightText: 2022 Deif Lou <ginoba@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <KisCanvasToolServices.h>

#include "KisLassoEnclosingProducer.h"

KisLassoEnclosingProducer::KisLassoEnclosingProducer(KoCanvasBase * canvas)
    : KisDynamicDelegateTool<KisToolOutlineBase>(canvas, KisToolOutlineBase::PAINT, Qt::ArrowCursor)
{
    setObjectName("enclosing_tool_lasso");
    setSupportOutline(true);
    setOutlineEnabled(false);

    KisCanvasToolServices *services = dynamic_cast<KisCanvasToolServices*>(canvas);
    KIS_ASSERT_RECOVER_RETURN(services);
    PkObject::connect(services->toolSignals(),
                      &KisCanvasToolSignals::effectiveCompositeOpChanged,
                      this, &KisLassoEnclosingProducer::resetCursorStyle);
}

KisLassoEnclosingProducer::~KisLassoEnclosingProducer()
{}

void  KisLassoEnclosingProducer::resetCursorStyle()
{
    if (isEraser()) {
        useCursor(Qt::ArrowCursor);
    } else {
        KisDynamicDelegateTool::resetCursorStyle();
    }

    overrideCursorIfNotEditable();
}

void KisLassoEnclosingProducer::enclosingMaskProduced(KisPixelSelectionSP enclosingMask)
{
    PkObject::activateSignal<KisPixelSelectionSP>(
        this,
        PkMemberFnKey::from(&KisLassoEnclosingProducer::enclosingMaskProduced),
        enclosingMask);
}

void KisLassoEnclosingProducer::finishOutline(const PkVector<PkPointF> &points)
{
    PkVector<PkPointF> pkPoints;
    pkPoints.reserve(points.size());
    for (const PkPointF &point : points) {
        pkPoints.append(point);
    }
    finishOutlinePk(pkPoints);
}

void KisLassoEnclosingProducer::finishOutlinePk(const PkVector<PkPointF> &points)
{
    if (points.size() < 3) {
        return;
    }
    
    KisPixelSelectionSP enclosingMask(new KisPixelSelection());

    KisPainter painter(enclosingMask);
    painter.setPaintColor(KoColor(Pk::white, enclosingMask->colorSpace()));
    painter.setAntiAliasPolygonFill(false);
    painter.setFillStyle(KisPainter::FillStyleForegroundColor);
    painter.setStrokeStyle(KisPainter::StrokeStyleNone);

    painter.paintPolygon(points);

    enclosingMaskProduced(enclosingMask);
}

bool KisLassoEnclosingProducer::hasUserInteractionRunning() const
{
    return m_hasUserInteractionRunning;
}

void KisLassoEnclosingProducer::beginShape()
{
    m_hasUserInteractionRunning = true;
}

void KisLassoEnclosingProducer::endShape()
{
    m_hasUserInteractionRunning = false;
}
