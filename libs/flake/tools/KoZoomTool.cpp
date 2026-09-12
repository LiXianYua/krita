/* This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2006-2007 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2006 Thorsten Zachmann <zachmann@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KoZoomTool.h"

#include "KoZoomStrategy.h"
#include "KoPointerEvent.h"
#include "KoCanvasBase.h"
#include "KoCanvasController.h"
#include "KoCanvasCursorHost.h"

#include <PkPoint.h>
#include <PkSize.h>

#include <FlakeDebug.h>

// 两个游标资源位图的尺寸。`:/zoom_in_cursor.png` / `:/zoom_out_cursor.png` 取自
// `libs/flake/pics/`（登记在 `libs/flake/flake.qrc`），PNG 头实测各为 32×32。
// 宿主按资源名载入游标真值，这里只需要同一份尺寸参与 token 身份——旧代码为此载入一次
// `QPixmap` 再读它的宽高。同目录同族的 `KoPathTool.cpp` 本来就是这个形态。
constexpr PkSize kZoomCursorSize(32, 32);

KoZoomTool::KoZoomTool(KoCanvasBase *canvas)
        : KoInteractionTool(canvas)
        , m_controller(nullptr)
        , m_zoomInMode(true)
{
    // 游标不再由工具自己持有平台对象：把「资源名 + 尺寸 + 热点」交给宿主，换回
    // 一个不可变快照的 token。尺寸取资源位图自身的尺寸（见 kZoomCursorSize）、
    // 热点固定 (4, 4)——与旧 QCursor(pixmap, 4, 4) 同义，不再构造任何游标对象。
    const auto *host = dynamic_cast<const KoCanvasCursorHost *>(canvas);
    if (!host) {
        return;
    }

    m_inCursor = host->loadCursorResource(PkString(":/zoom_in_cursor.png"),
                                          kZoomCursorSize,
                                          PkPoint(4, 4));
    m_outCursor = host->loadCursorResource(PkString(":/zoom_out_cursor.png"),
                                           kZoomCursorSize,
                                           PkPoint(4, 4));
}

void KoZoomTool::mouseReleaseEvent(KoPointerEvent *event)
{
    KoInteractionTool::mouseReleaseEvent(event);
}

void KoZoomTool::mouseMoveEvent(KoPointerEvent *event)
{
    updateCursor(event->modifiers() & Pk::ControlModifier);

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
    if (event->button() == Pk::RightButton ||
        event->modifiers() == Pk::ControlModifier) {
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
