/*
 *  SPDX-FileCopyrightText: 2007 Sven Langkamp <sven.langkamp@gmail.com>
 *  SPDX-FileCopyrightText: 2010 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_tool_path.h"
#include <QMouseEvent>
#include <QTabletEvent>
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
            this, [this](int key, const QVariant &) {
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
    Q_UNUSED(event)
}

// Install an event filter to catch right-click events.
// The simplest way to accommodate the popup palette binding.
// This code is duplicated in kis_tool_select_path.cc
bool KisToolPath::eventFilter(QObject *obj, QEvent *event)
{
    Q_UNUSED(obj);
    if (!localTool()->pathStarted()) {
        return false;
    }
    if (event->type() == QEvent::MouseButtonPress ||
            event->type() == QEvent::MouseButtonDblClick) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::RightButton) {
            localTool()->removeLastPoint();
            return true;
        }
    } else if (event->type() == QEvent::TabletPress) {
        QTabletEvent *tabletEvent = static_cast<QTabletEvent*>(event);
        if (tabletEvent->button() == Qt::RightButton) {
            localTool()->removeLastPoint();
            return true;
        }
    }
    return false;
}

void KisToolPath::beginAlternateAction(KoPointerEvent *event, AlternateAction action) {
    DelegatedPathTool::beginAlternateAction(event, action);
    if (!nodeEditable()) return;

    if (nodePaintAbility() == KisToolPath::MYPAINTBRUSH_UNPAINTABLE) {
        KisCanvasFeedback *feedback = dynamic_cast<KisCanvasFeedback*>(canvas());
        KIS_SAFE_ASSERT_RECOVER(feedback) {
            event->ignore();
            return;
        }
        QString message = i18n("The MyPaint Brush Engine is not available for this colorspace");
        feedback->showFloatingMessage(message, koIcon("object-locked"), 4500,
                                      KisCanvasFeedback::Priority::Medium,
                                      Qt::AlignCenter | Qt::TextWordWrap);
        event->ignore();
        return;
    }
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

void __KisToolPathLocalTool::paintPath(KoPathShape &pathShape, QPainter &painter, const KoViewConverter &converter)
{
    Q_UNUSED(converter);

    QTransform matrix;
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
