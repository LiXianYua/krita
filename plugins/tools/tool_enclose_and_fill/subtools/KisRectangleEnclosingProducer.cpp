/*
 * KDE. Krita Project.
 *
 * SPDX-FileCopyrightText: 2022 Deif Lou <ginoba@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <KoCanvasResourceProvider.h>
#include <PkTransform.h>

#include "KisRectangleEnclosingProducer.h"

KisRectangleEnclosingProducer::KisRectangleEnclosingProducer(KoCanvasBase * canvas)
    : KisDynamicDelegateTool<KisToolRectangleBase>(canvas, KisToolRectangleBase::PAINT, Qt::ArrowCursor)
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

KisRectangleEnclosingProducer::~KisRectangleEnclosingProducer()
{}

void  KisRectangleEnclosingProducer::resetCursorStyle()
{
    if (isEraser()) {
        useCursor(Qt::ArrowCursor);
    } else {
        KisDynamicDelegateTool::resetCursorStyle();
    }

    overrideCursorIfNotEditable();
}

void KisRectangleEnclosingProducer::enclosingMaskProduced(KisPixelSelectionSP enclosingMask)
{
    PkObject::activateSignal<KisPixelSelectionSP>(
        this,
        PkMemberFnKey::from(&KisRectangleEnclosingProducer::enclosingMaskProduced),
        enclosingMask);
}

void KisRectangleEnclosingProducer::finishRect(const PkRectF& rect, qreal roundCornersX, qreal roundCornersY)
{
    PkRect rc(rect.normalized().toRect());
    if (!rc.isValid()) {
        return;
    }

    KisPixelSelectionSP enclosingMask = KisPixelSelectionSP(new KisPixelSelection());
    PkPainterPath path;

    if (roundCornersX > 0 || roundCornersY > 0) {
        path.addRoundedRect(PkRectF(rc), roundCornersX, roundCornersY);
    } else {
        path.addRect(PkRectF(rc));
    }
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

bool KisRectangleEnclosingProducer::hasUserInteractionRunning() const
{
    return m_hasUserInteractionRunning;
}

void KisRectangleEnclosingProducer::beginShape()
{
    m_hasUserInteractionRunning = true;
}

void KisRectangleEnclosingProducer::endShape()
{
    m_hasUserInteractionRunning = false;
}
