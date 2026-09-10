/* This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2006-2007 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2006 Thorsten Zachmann <zachmann@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KoZoomTool.h"

#include <QPixmap>

#include "KoZoomStrategy.h"
#include "KoPointerEvent.h"
#include "KoCanvasBase.h"
#include "KoCanvasController.h"
#include "KoCanvasCursorHost.h"

#include <PkPoint.h>
#include <PkSize.h>

#include <FlakeDebug.h>

KoZoomTool::KoZoomTool(KoCanvasBase *canvas)
        : KoInteractionTool(canvas)
        , m_controller(nullptr)
        , m_zoomInMode(true)
{
    // 游标不再由工具自己持有平台对象：把「资源名 + 尺寸 + 热点」交给宿主，换回
    // 一个不可变快照的 token。尺寸仍取资源位图自身的尺寸、热点固定 (4, 4)——与
    // 旧 QCursor(pixmap, 4, 4) 同义；这里读一次位图只是为了让宿主拿到同一份尺寸，
    // 不再构造任何游标对象。
    const auto *host = dynamic_cast<const KoCanvasCursorHost *>(canvas);
    if (!host) {
        return;
    }

    QPixmap inPixmap, outPixmap;
    inPixmap.load(":/zoom_in_cursor.png");
    outPixmap.load(":/zoom_out_cursor.png");
    m_inCursor = host->loadCursorResource(PkString(":/zoom_in_cursor.png"),
                                          PkSize(inPixmap.width(), inPixmap.height()),
                                          PkPoint(4, 4));
    m_outCursor = host->loadCursorResource(PkString(":/zoom_out_cursor.png"),
                                           PkSize(outPixmap.width(), outPixmap.height()),
                                           PkPoint(4, 4));
}

void KoZoomTool::mouseReleaseEvent(KoPointerEvent *event)
{
    KoInteractionTool::mouseReleaseEvent(event);
}

void KoZoomTool::mouseMoveEvent(KoPointerEvent *event)
{
    updateCursor(event->modifiers() & Qt::ControlModifier);

    KoInteractionTool::mouseMoveEvent(event);
}

void KoZoomTool::pkKeyPressEvent(PkToolKeyEvent *event)
{
    event->ignore();
    updateCursor(event->modifiers() & Pk::ControlModifier);

    KoInteractionTool::pkKeyPressEvent(event);
}

void KoZoomTool::pkKeyReleaseEvent(PkToolKeyEvent *event)
{
    event->ignore();
    updateCursor(event->modifiers() & Pk::ControlModifier);

    KoInteractionTool::pkKeyReleaseEvent(event);
}

void KoZoomTool::activate(const PkSet<KoShape*> &)
{
    updateCursor(false);
}

void KoZoomTool::mouseDoubleClickEvent(KoPointerEvent *event)
{
    mousePressEvent(event);
}

KoInteractionStrategy *KoZoomTool::createStrategy(KoPointerEvent *event)
{
    KoZoomStrategy *zs = new KoZoomStrategy(this, m_controller, event->point);
    bool shouldZoomIn = m_zoomInMode;
    if (event->button() == Qt::RightButton ||
        event->modifiers() == Qt::ControlModifier) {
        shouldZoomIn = !shouldZoomIn;
    }

    if (shouldZoomIn) {
        zs->forceZoomIn();
    } else {
        zs->forceZoomOut();
    }
    return zs;
}

void KoZoomTool::setZoomInMode(bool zoomIn)
{
    m_zoomInMode = zoomIn;
    updateCursor(false);
}

void KoZoomTool::updateCursor(bool swap)
{
    bool setZoomInCursor = m_zoomInMode;
    if (swap) {
        setZoomInCursor = !setZoomInCursor;
    }

    if (setZoomInCursor) {
        useCursor(m_inCursor);
    } else {
        useCursor(m_outCursor);
    }
}
