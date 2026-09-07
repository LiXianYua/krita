/*
 * KDE. Krita Project.
 *
 * SPDX-FileCopyrightText: 2022 Deif Lou <ginoba@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <KoCanvasResourceProvider.h>
#include <PkTransform.h>

#include "KisEllipseEnclosingProducer.h"

KisEllipseEnclosingProducer::KisEllipseEnclosingProducer(KoCanvasBase * canvas)
    : KisDynamicDelegateTool<KisToolEllipseBase>(canvas, KisToolEllipseBase::PAINT, Qt::ArrowCursor)
{
    QObject::setObjectName("enclosing_tool_rectangle");
    setSupportOutline(true);
    setOutlineEnabled(false);

    QObject::connect(canvas->resourceManager(), &KoCanvasResourceProvider::canvasResourceChanged,
            this, [this](int key, const PkVariant &) {
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

void KisEllipseEnclosingProducer::enclosingMaskProduced(KisPixelSelectionSP enclosingMask)
{
    PkObject::activateSignal<KisPixelSelectionSP>(
        this,
        PkMemberFnKey::from(&KisEllipseEnclosingProducer::enclosingMaskProduced),
        enclosingMask);
}

void KisEllipseEnclosingProducer::finishRect(const PkRectF& rect, qreal roundCornersX, qreal roundCornersY)
{
    (void)roundCornersX;
    (void)roundCornersY;
    
    PkRect rc(rect.normalized().toRect());
    if (!rc.isValid()) {
        return;
    }

    KisPixelSelectionSP enclosingMask = KisPixelSelectionSP(new KisPixelSelection());
    PkPainterPath path;

    path.addEllipse(PkRectF(rc));
    PkTransform rotation;
    rotation.translate(rc.center().x(), rc.center().y());
    rotation.rotateRadians(getRotationAngle());
    rotation.translate(-rc.center().x(), -rc.center().y());
    path = rotation.map(path);

    KisPainter painter(enclosingMask);
    painter.setPaintColor(KoColor(Pk::white, enclosingMask->colorSpace()));
    painter.setAntiAliasPolygonFill(false);
    painter.setFillStyle(KisPainter::FillStyleForegroundColor);
    painter.setStrokeStyle(KisPainter::StrokeStyleNone);

    painter.paintPainterPath(path);

    enclosingMaskProduced(enclosingMask);
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
