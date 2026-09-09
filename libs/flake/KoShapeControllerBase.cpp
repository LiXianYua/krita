/* This file is part of the KDE project

   SPDX-FileCopyrightText: 2006, 2010 Thomas Zander <zander@kde.org>
   SPDX-FileCopyrightText: 2011 Jan Hambrecht <jaham@gmx.net>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <PkConfigGroup.h>
#include <PkSharedConfig.h>
#include <PkTransform.h>

#include "KoShapeControllerBase.h"
#include "KoDocumentResourceManager.h"
#include "KoShapeRegistry.h"
#include "KoShapeFactoryBase.h"

#include <kundo2command.h>

class KoshapeControllerBasePrivate
{
public:
    KoshapeControllerBasePrivate()
        : resourceManager(new KoDocumentResourceManager())
    {
        KoShapeRegistry *registry = KoShapeRegistry::instance();
        for (const PkString &id : registry->keys()) {
            KoShapeFactoryBase *shapeFactory = registry->value(id);
            shapeFactory->newDocumentResourceManager(resourceManager);
        }
        // read persistent application wide resources
        PkSharedConfig *config = PkSharedConfig::openConfig();
        PkConfigGroup miscGroup = config->group("Misc");
        const uint grabSensitivity = miscGroup.readEntry("GrabSensitivity", 10);
        resourceManager->setGrabSensitivity(grabSensitivity);
    }

    ~KoshapeControllerBasePrivate()
    {
        delete resourceManager;
    }

    KoDocumentResourceManager *resourceManager;
};

KoShapeControllerBase::KoShapeControllerBase()
    : d(new KoshapeControllerBasePrivate())
{
}

KoShapeControllerBase::~KoShapeControllerBase()
{
    delete d;
}

KoShapeContainer* KoShapeControllerBase::createParentForShapes(const PkList<KoShape*> shapes, bool forceNewLayer, KUndo2Command *parentCommand)
{
    (void)parentCommand;
    (void)forceNewLayer;
    (void)shapes;

    return 0;
}

KoDocumentResourceManager *KoShapeControllerBase::resourceManager() const
{
    return d->resourceManager;
}

PkRectF KoShapeControllerBase::documentRect() const
{
    const qreal pxToPt = 72.0 / pixelsPerInch();

    PkTransform t = PkTransform::fromScale(pxToPt, pxToPt);
    return t.mapRect(documentRectInPixels());
}
