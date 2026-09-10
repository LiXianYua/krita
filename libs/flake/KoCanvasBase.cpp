/* This file is part of the KDE project

   SPDX-FileCopyrightText: 2006 Thorsten Zachmann <zachmann@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <PkRect.h>
#include <PkObject.h>
#include <PkThreadCallQueue.h>
#include <QDebug>

#include <utility>

#include "KoCanvasBase.h"
#include "KoCanvasResourceProvider.h"
#include "KoShapeController.h"
#include "KoCanvasController.h"
#include "KoViewConverter.h"
#include "KoSnapGuide.h"
#include "KoShapeManager.h"
#include "KoToolProxy.h"
#include "KoSelection.h"
#include "KoSelectedShapesProxy.h"

class Q_DECL_HIDDEN KoCanvasBase::Private
{
public:
    Private()
        : shapeController(0),
        resourceManager(0),
        isResourceManagerShared(false),
        controller(0),
        snapGuide(0)
    {
    }

    ~Private() {
        delete shapeController;
        if (!isResourceManagerShared) {
            delete resourceManager;
        }
        delete snapGuide;
    }
    KoShapeController *shapeController;
    KoCanvasResourceProvider *resourceManager;
    bool isResourceManagerShared;
    KoCanvasController *controller;
    KoSnapGuide *snapGuide;
    PkObject deferredCallContext;
};

KoCanvasBase::KoCanvasBase(KoShapeControllerBase *shapeController, KoCanvasResourceProvider *sharedResourceManager)
        : PkObject()
        , d(new Private())
{
    d->resourceManager = sharedResourceManager ?
        sharedResourceManager : new KoCanvasResourceProvider();
    d->isResourceManagerShared = sharedResourceManager;

    d->shapeController = new KoShapeController(this, shapeController);
    d->snapGuide = new KoSnapGuide(this);
}

KoCanvasBase::~KoCanvasBase()
{
    d->shapeController->reset();
    delete d;
}

PkPointF KoCanvasBase::viewToDocument(const PkPointF &viewPoint) const
{
    return viewConverter()->viewToDocument(viewPoint - documentOrigin());
}

KoShapeController *KoCanvasBase::shapeController() const
{
    if (d->shapeController)
        return d->shapeController;
    else
        return 0;
}

void KoCanvasBase::disconnectCanvasObserver(PkObject *object)
{
    if (shapeManager()) {
        PkObject::disconnect(shapeManager()->selection(), nullptr, object, nullptr);
    }
    if (resourceManager()) {
        PkObject::disconnect(resourceManager(), nullptr, object, nullptr);
    }
    if (shapeManager()) PkObject::disconnect(shapeManager(), nullptr, object, nullptr);
    if (toolProxy()) {
        PkObject::disconnect(toolProxy(), nullptr, object, nullptr);
    }
    if (selectedShapesProxy()) {
        PkObject::disconnect(selectedShapesProxy(), nullptr, object, nullptr);
    }
}


KoCanvasResourceProvider *KoCanvasBase::resourceManager() const
{
    return d->resourceManager;
}

void KoCanvasBase::setCanvasController(KoCanvasController *controller)
{
    d->controller = controller;
}

KoCanvasController *KoCanvasBase::canvasController() const
{
    return d->controller;
}

void KoCanvasBase::clipToDocument(const KoShape *, PkPointF &) const
{
}

KoSnapGuide * KoCanvasBase::snapGuide() const
{
    return d->snapGuide;
}

void KoCanvasBase::postDeferredCall(std::function<void()> callback) const
{
    PkObject *context = deferredCallContext();
    PkThreadCallQueue::post(context->thread(),
                            std::move(callback),
                            context->callLifetime());
}

PkObject *KoCanvasBase::deferredCallContext() const
{
    return &d->deferredCallContext;
}
