/*
 *  SPDX-FileCopyrightText: 2007 Sven Langkamp <sven.langkamp@gmail.com>
 *  SPDX-FileCopyrightText: 2010 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_tool_path.h"
#include <KoPathShape.h>
#include <KoCanvasBase.h>
#include <KoCanvasResourceProvider.h>
#include <KisCanvasToolServices.h>
#include <KisCanvasFeedback.h>


KisToolPath::KisToolPath(KoCanvasBase * canvas)
    : DelegatedPathTool(canvas, Qt::ArrowCursor,
                        new __KisToolPathLocalTool(canvas, this))
{
    setIsOpacityPresetMode(true);
    connect(canvas->resourceManager(), &KoCanvasResourceProvider::canvasResourceChanged,
            this, [this](int key, const PkVariant &) {
                if (key == KoCanvasResource::CurrentEffectiveCompositeOp) {
                    resetCursorStyle();
                }
            });

}

void KisToolPath::resetCursorStyle()
{
    if (isEraser() && (nodePaintAbility() == PAINT)) {
        useCursor(dynamic_cast<KisCanvasToolServices *>(canvas())->toolCursor(CURSOR_STYLE_ERASER));
    } else {
        DelegatedPathTool::resetCursorStyle();
    }

    overrideCursorIfNotEditable();
}

void KisToolPath::requestStrokeEnd()
{
    localTool()->endPathWithoutLastPoint();
}

void KisToolPath::requestStrokeCancellation()
{
    localTool()->cancelPath();
}

KisPopupWidgetInterface* KisToolPath::popupWidget()
{
    return localTool()->pathStarted() ? nullptr : DelegatedPathTool::popupWidget();
}

void KisToolPath::mousePressEvent(KoPointerEvent *event)
{
    (void)event;
}

void KisToolPath::beginAlternateAction(KoPointerEvent *event, AlternateAction action)
{
    if (localTool()->pathStarted() && action == Secondary) {
        localTool()->removeLastPoint();
        return;
    }

    DelegatedPathTool::beginAlternateAction(event, action);
    if (!nodeEditable()) return;

    if (nodePaintAbility() == KisToolPath::MYPAINTBRUSH_UNPAINTABLE) {
        KisCanvasFeedback *feedback = dynamic_cast<KisCanvasFeedback*>(canvas());
        KIS_SAFE_ASSERT_RECOVER(feedback) {
            event->ignore();
            return;
        }
        PkString message("The MyPaint Brush Engine is not available for this colorspace");
        feedback->showFloatingMessage(message, {}, 4500,
                                      KisCanvasFeedback::Priority::Medium,
                                      Qt::AlignCenter | Qt::TextWordWrap);
        event->ignore();
        return;
    }
}

void KisToolPath::beginAlternateDoubleClickAction(KoPointerEvent *event, AlternateAction action)
{
    if (localTool()->pathStarted() && action == Secondary) {
        localTool()->removeLastPoint();
        return;
    }

    DelegatedPathTool::beginAlternateDoubleClickAction(event, action);
}

void KisToolPath::beginPrimaryAction(KoPointerEvent* event)
{
    if (!nodeEditable()) return;
    DelegatedPathTool::mousePressEvent(event);
}

void KisToolPath::continuePrimaryAction(KoPointerEvent *event)
{
    mouseMoveEvent(event);
}

void KisToolPath::endPrimaryAction(KoPointerEvent *event)
{
    mouseReleaseEvent(event);
}

void KisToolPath::beginPrimaryDoubleClickAction(KoPointerEvent *event)
{
    DelegatedPathTool::mouseDoubleClickEvent(event);
}

__KisToolPathLocalTool::__KisToolPathLocalTool(KoCanvasBase * canvas, KisToolPath* parentTool)
    : KoCreatePathTool(canvas)
    , m_parentTool(parentTool) {
    setIsOpacityPresetMode(true);
}

void __KisToolPathLocalTool::paintPath(KoPathShape &pathShape, PkPainter &painter, const KoViewConverter &converter)
{
    (void)converter;

    PkTransform matrix;
    matrix.scale(m_parentTool->image()->xRes(), m_parentTool->image()->yRes());
    matrix.translate(pathShape.position().x(), pathShape.position().y());
    m_parentTool->paintToolOutline(&painter, m_parentTool->pixelToView(matrix.map(pathShape.outline())));
}

void __KisToolPathLocalTool::addPathShape(KoPathShape* pathShape)
{
    if (!KoCreatePathTool::tryMergeInPathShape(pathShape)) {
        m_parentTool->addPathShape(pathShape, kundo2_i18n("Draw Bezier Curve"));
    }
}
