/*
 * KDE. Krita Project.
 *
 * SPDX-FileCopyrightText: 2022 Deif Lou <ginoba@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <KoCanvasResourceProvider.h>
#include <KoPathShape.h>
#include <KisCanvasFeedback.h>
#include <PkTransform.h>

#include "KisPathEnclosingProducer.h"

KisToolPathLocalTool::KisToolPathLocalTool(KoCanvasBase * canvas, KisPathEnclosingProducer* parentTool)
    : KoCreatePathTool(canvas)
    , m_parentTool(parentTool)
{}

void KisToolPathLocalTool::paintPath(KoPathShape &pathShape, PkPainter &painter, const KoViewConverter &converter)
{
    (void)converter;

    PkTransform matrix;
    matrix.scale(m_parentTool->image()->xRes(), m_parentTool->image()->yRes());
    matrix.translate(pathShape.position().x(), pathShape.position().y());
    m_parentTool->paintToolOutline(&painter, m_parentTool->pixelToView(matrix.map(pathShape.outline())));
}

void KisToolPathLocalTool::addPathShape(KoPathShape* pathShape)
{
    m_parentTool->addPathShape(pathShape);
}

void KisToolPathLocalTool::beginShape()
{
    m_parentTool->beginShape();
}

void KisToolPathLocalTool::endShape()
{
    m_parentTool->endShape();
}

KisPathEnclosingProducer::KisPathEnclosingProducer(KoCanvasBase * canvas)
    : KisDynamicDelegateTool<DelegatedPathTool>(canvas,
                                                Qt::ArrowCursor,
                                                new KisToolPathLocalTool(canvas, this))
{
    setObjectName("enclosing_tool_path");
    setSupportOutline(true);
    setOutlineEnabled(false);

    connect(canvas->resourceManager(), &KoCanvasResourceProvider::canvasResourceChanged,
            this, [this](int key, const PkVariant &) {
                if (key == KoCanvasResource::CurrentEffectiveCompositeOp) {
                    resetCursorStyle();
                }
            });
}

KisPathEnclosingProducer::~KisPathEnclosingProducer()
{}

void  KisPathEnclosingProducer::resetCursorStyle()
{
    if (isEraser()) {
        useCursor(Qt::ArrowCursor);
    } else {
        KisDynamicDelegateTool::resetCursorStyle();
    }

    overrideCursorIfNotEditable();
}

void KisPathEnclosingProducer::requestStrokeEnd()
{
    KisDynamicDelegateTool::requestStrokeEnd();
    localTool()->endPathWithoutLastPoint();
}

void KisPathEnclosingProducer::requestStrokeCancellation()
{
    KisDynamicDelegateTool::requestStrokeCancellation();
    localTool()->cancelPath();
}

KisPopupWidgetInterface* ::KisPathEnclosingProducer::popupWidget()
{
    return m_hasUserInteractionRunning ? nullptr : KisDynamicDelegateTool::popupWidget();
}

void KisPathEnclosingProducer::mousePressEvent(KoPointerEvent *event)
{
    (void)event;
}

void KisPathEnclosingProducer::beginAlternateAction(KoPointerEvent *event, AlternateAction action) {
    KisDynamicDelegateTool::beginAlternateAction(event, action);
    if (!nodeEditable()) return;

    if (nodePaintAbility() == KisDynamicDelegateTool::MYPAINTBRUSH_UNPAINTABLE) {
        KisCanvasFeedback *feedback = dynamic_cast<KisCanvasFeedback*>(canvas());
        KIS_SAFE_ASSERT_RECOVER_RETURN(feedback);
        PkString message("The MyPaint Brush Engine is not available for this colorspace");
        feedback->showFloatingMessage(message, {});
        event->ignore();
        return;
    }
}

void KisPathEnclosingProducer::beginPrimaryAction(KoPointerEvent* event)
{
    if (!nodeEditable()) return;
    KisDynamicDelegateTool::mousePressEvent(event);
}

void KisPathEnclosingProducer::continuePrimaryAction(KoPointerEvent *event)
{
    mouseMoveEvent(event);
}

void KisPathEnclosingProducer::endPrimaryAction(KoPointerEvent *event)
{
    mouseReleaseEvent(event);
}

void KisPathEnclosingProducer::beginPrimaryDoubleClickAction(KoPointerEvent *event)
{
    KisDynamicDelegateTool::mouseDoubleClickEvent(event);
}

void KisPathEnclosingProducer::addPathShape(KoPathShape* pathShape)
{
    KisImageWSP currentImage = image();
    if (!currentImage) {
        return;
    }

    KisPixelSelectionSP enclosingMask(new KisPixelSelection());

    pathShape->normalize();
    pathShape->close();

    KisPainter painter(enclosingMask);
    painter.setPaintColor(KoColor(Qt::white, enclosingMask->colorSpace()));
    painter.setAntiAliasPolygonFill(false);
    painter.setFillStyle(KisPainter::FillStyleForegroundColor);
    painter.setStrokeStyle(KisPainter::StrokeStyleNone);

    PkTransform matrix;
    matrix.scale(currentImage->xRes(), currentImage->yRes());
    matrix.translate(pathShape->position().x(), pathShape->position().y());

    PkPainterPath path = matrix.map(pathShape->outline());
    painter.fillPainterPath(path);
    enclosingMask->setOutlineCache(path);

    delete pathShape;

    enclosingMaskProduced(enclosingMask);
}

void KisPathEnclosingProducer::enclosingMaskProduced(KisPixelSelectionSP enclosingMask)
{
    PkObject::activateSignal<KisPixelSelectionSP>(
        this,
        PkMemberFnKey::from(&KisPathEnclosingProducer::enclosingMaskProduced),
        enclosingMask);
}

bool KisPathEnclosingProducer::hasUserInteractionRunning() const
{
    return m_hasUserInteractionRunning;
}

void KisPathEnclosingProducer::beginShape()
{
    m_hasUserInteractionRunning = true;
}

void KisPathEnclosingProducer::endShape()
{
    m_hasUserInteractionRunning = false;
}
