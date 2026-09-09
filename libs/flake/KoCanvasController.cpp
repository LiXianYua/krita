/* This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2010 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KoCanvasController.h"
#include "KoToolManager.h"

#include <PkSize.h>
#include <PkPoint.h>

class Q_DECL_HIDDEN KoCanvasController::Private
{
public:
    Private()
        : preferredCenterFractionX(0.5)
        , preferredCenterFractionY(0.5)
        , actionCollection(0)
    {
    }

    PkSizeF documentSize;
    PkPoint documentOffset;
    qreal preferredCenterFractionX;
    qreal preferredCenterFractionY;
    QObject *actionCollection;
};

KoCanvasController::KoCanvasController(QObject *actionCollection)
    : d(new Private())
{
    proxyObject = new KoCanvasControllerProxyObject(this);
    d->actionCollection = actionCollection;
}

KoCanvasController::~KoCanvasController()
{
    KoToolManager::instance()->removeCanvasController(this);
    delete d;
    delete proxyObject;
}

KoCanvasBase* KoCanvasController::canvas() const
{
    return 0;
}

KoCanvasControllerProxyObject::KoCanvasControllerProxyObject(KoCanvasController *controller, PkObject *parent)
    : PkObject(parent)
    , m_canvasController(controller)
{
}

QObject *KoCanvasController::actionCollection() const
{
    return d->actionCollection;
}
