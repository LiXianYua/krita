/*
 * KDE. Krita Project.
 *
 * SPDX-FileCopyrightText: 2022 Deif Lou <ginoba@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <KoCanvasResourceProvider.h>

#include "KisEllipseEnclosingProducer.h"

KisEllipseEnclosingProducer::KisEllipseEnclosingProducer(KoCanvasBase * canvas)
    : KisDynamicDelegateTool<KisToolEllipseBase>(canvas, KisToolEllipseBase::PAINT, Qt::ArrowCursor)
{
    setObjectName("enclosing_tool_rectangle");
    setSupportOutline(true);
    setOutlineEnabled(false);

    connect(canvas->resourceManager(), &KoCanvasResourceProvider::canvasResourceChanged,
            this, [this](int key, const QVariant &) {
                if (key == KoCanvasResource::CurrentEffectiveCompositeOp) {
                    resetCursorStyle();
                }
            });
}

KisEllipseEnclosingProducer::~KisEllipseEnclosingProducer()
{}

void  KisEllipseEnclosingProducer::resetCursorStyle()
{
    if (isEraser()) {
        useCursor(Qt::ArrowCursor);
    } else {
        KisDynamicDelegateTool::resetCursorStyle();
    }

    overrideCursorIfNotEditable();
}

void KisEllipseEnclosingProducer::finishRect(const QRectF& rect, qreal roundCornersX, qreal roundCornersY)
{
    Q_UNUSED(roundCornersX);
    Q_UNUSED(roundCornersY);
    
    QRect rc(rect.normalized().toRect());
    if (!rc.isValid()) {
        return;
    }

    KisPixelSelectionSP enclosingMask = KisPixelSelectionSP(new KisPixelSelection());
    QPainterPath path;

    path.addEllipse(rc);
    getRotatedPath(path, rc.center(), getRotationAngle());

    KisPainter painter(enclosingMask);
    painter.setPaintColor(KoColor(Qt::white, enclosingMask->colorSpace()));
    painter.setAntiAliasPolygonFill(false);
    painter.setFillStyle(KisPainter::FillStyleForegroundColor);
    painter.setStrokeStyle(KisPainter::StrokeStyleNone);

    painter.paintPainterPath(path);

    Q_EMIT enclosingMaskProduced(enclosingMask);
}

bool KisEllipseEnclosingProducer::hasUserInteractionRunning() const
{
    return m_hasUserInteractionRunning;
}

void KisEllipseEnclosingProducer::beginShape()
{
    m_hasUserInteractionRunning = true;
}

void KisEllipseEnclosingProducer::endShape()
{
    m_hasUserInteractionRunning = false;
}
