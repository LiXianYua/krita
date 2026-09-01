/*
 *  SPDX-FileCopyrightText: 2012 Sven Langkamp <sven.langkamp@gmail.com>
 *  SPDX-FileCopyrightText: 2010 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_tool_pencil.h"
#include <KoPathShape.h>
#include <KoCanvasBase.h>
#include <KoCanvasResourceProvider.h>
#include <KoPointerEvent.h>
#include <KoShapeStroke.h>
#include <KisCanvasFeedback.h>

#include <KisCanvasToolServices.h>

KisToolPencil::KisToolPencil(KoCanvasBase * canvas)
    : DelegatedPencilTool(canvas, Qt::ArrowCursor,
                          new __KisToolPencilLocalTool(canvas, this))
{
    setIsOpacityPresetMode(true);
    connect(canvas->resourceManager(), &KoCanvasResourceProvider::canvasResourceChanged,
            this, [this](int key, const PkVariant &) {
                if (key == KoCanvasResource::CurrentEffectiveCompositeOp) {
                    resetCursorStyle();
                }
            });
}

void KisToolPencil::resetCursorStyle()
{
    if (isEraser() && (nodePaintAbility() == PAINT)) {
        useCursor(dynamic_cast<KisCanvasToolServices *>(canvas())->toolCursor(CURSOR_STYLE_ERASER));
    } else {
        DelegatedPencilTool::resetCursorStyle();
    }

    overrideCursorIfNotEditable();
}

void KisToolPencil::updatePencilCursor(bool value)
{
    if (mode() == HOVER_MODE || mode() == PAINT_MODE) {
        setCursor(value ? Qt::ArrowCursor : Qt::ForbiddenCursor);
        resetCursorStyle();
    }
}

void KisToolPencil::mousePressEvent(KoPointerEvent *event)
{
    (void)event;
}

void KisToolPencil::mouseDoubleClickEvent(KoPointerEvent *event)
{
    (void)event;
}

void KisToolPencil::beginPrimaryAction(KoPointerEvent *event)
{
    if (!nodeEditable()) return;

    if (nodePaintAbility() == KisToolPencil::MYPAINTBRUSH_UNPAINTABLE) {
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

    DelegatedPencilTool::mousePressEvent(event);
}

void KisToolPencil::continuePrimaryAction(KoPointerEvent *event)
{
    mouseMoveEvent(event);
}

void KisToolPencil::endPrimaryAction(KoPointerEvent *event)
{
    mouseReleaseEvent(event);
}

__KisToolPencilLocalTool::__KisToolPencilLocalTool(KoCanvasBase * canvas, KisToolPencil* parentTool)
    : KoPencilTool(canvas), m_parentTool(parentTool) {
    setIsOpacityPresetMode(true);
}

void __KisToolPencilLocalTool::paint(PkPainter &painter, const KoViewConverter &converter)
{
    if (m_parentTool->strokeStyle() == KisToolShapeUtils::StrokeStyleNone) {
        paintPath(path(), painter, converter);
    } else {
        KoPencilTool::paint(painter, converter);
    }
}



void __KisToolPencilLocalTool::paintPath(KoPathShape *pathShape, PkPainter &painter, const KoViewConverter &converter)
{
    (void)converter;
    if (!pathShape) {
        return;
    }

    PkTransform matrix;
    matrix.scale(m_parentTool->image()->xRes(), m_parentTool->image()->yRes());
    matrix.translate(pathShape->position().x(), pathShape->position().y());
    m_parentTool->paintToolOutline(&painter, m_parentTool->pixelToView(matrix.map(pathShape->outline())));
}

void __KisToolPencilLocalTool::addPathShape(KoPathShape* pathShape, bool closePath)
{
    if (closePath) {
        pathShape->close();
        pathShape->normalize();
    }

    m_parentTool->addPathShape(pathShape, kundo2_i18n("Draw Freehand Path"));
}

void __KisToolPencilLocalTool::slotUpdatePencilCursor()
{
    auto style = m_parentTool->strokeStyle();
    if (style ==  KisToolShapeUtils::StrokeStyleForeground )
    {
        KoPencilTool::setStrokeColor(canvas()->resourceManager()->foregroundColor().toQColor());
    }
    else if ( style == KisToolShapeUtils::StrokeStyleBackground)
    {
        KoPencilTool::setStrokeColor(canvas()->resourceManager()->backgroundColor().toQColor());
    }

    KoShapeStrokeSP stroke = this->createStroke();
    m_parentTool->updatePencilCursor(stroke && stroke->isVisible());
}
