/*
   SPDX-FileCopyrightText: 2006 Boudewijn Rempt (boud@valdyas.org)
   SPDX-FileCopyrightText: 2007, 2010 Thomas Zander <zander@kde.org>
   SPDX-FileCopyrightText: 2008 Carlos Licea <carlos.licea@kdemail.net>
   SPDX-FileCopyrightText: 2011 Jan Hambrecht <jaham@gmx.net>

   SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include "KoDocumentResourceManager.h"

#include <PkVariant.h>
#include <kundo2stack.h>
#include <FlakeDebug.h>

#include "KoShape.h"
#include "KoShapeController.h"
#include "KoResourceManager_p.h"

class KoDocumentResourceManager::Private
{
public:
    KoResourceManager manager;
};

KoDocumentResourceManager::KoDocumentResourceManager(PkObject *parent)
        : PkObject(parent),
        d(new Private())
{
    PkObject::connect(&d->manager, &KoResourceManager::resourceChanged,
            this, &KoDocumentResourceManager::resourceChanged);
}

KoDocumentResourceManager::~KoDocumentResourceManager()
{
    delete d;
}

void KoDocumentResourceManager::resourceChanged(int key, const PkVariant &value)
{
    activateSignal<int, const PkVariant &>(
        this, PkMemberFnKey::from(&KoDocumentResourceManager::resourceChanged), key, value);
}

void KoDocumentResourceManager::setResource(int key, const PkVariant &value)
{
    d->manager.setResource(key, value);
}

PkVariant KoDocumentResourceManager::resource(int key) const
{
    return d->manager.resource(key);
}

void KoDocumentResourceManager::setResource(int key, const KoColor &color)
{
    PkVariant v;
    v.setValue(color);
    setResource(key, v);
}

void KoDocumentResourceManager::setResource(int key, KoShape *shape)
{
    PkVariant v;
    v.setValue(shape);
    setResource(key, v);
}

void KoDocumentResourceManager::setResource(int key, const KoUnit &unit)
{
    PkVariant v;
    v.setValue(unit);
    setResource(key, v);
}

KoColor KoDocumentResourceManager::koColorResource(int key) const
{
    return d->manager.koColorResource(key);
}


bool KoDocumentResourceManager::boolResource(int key) const
{
    return d->manager.boolResource(key);
}

int KoDocumentResourceManager::intResource(int key) const
{
    return d->manager.intResource(key);
}

PkString KoDocumentResourceManager::stringResource(int key) const
{
    return d->manager.stringResource(key);
}

PkSizeF KoDocumentResourceManager::sizeResource(int key) const
{
    return d->manager.sizeResource(key);
}

bool KoDocumentResourceManager::hasResource(int key) const
{
    return d->manager.hasResource(key);
}

void KoDocumentResourceManager::clearResource(int key)
{
    d->manager.clearResource(key);
}

KUndo2Stack *KoDocumentResourceManager::undoStack() const
{
    if (!hasResource(UndoStack))
        return 0;
    return static_cast<KUndo2Stack*>(resource(UndoStack).value<void*>());
}

void KoDocumentResourceManager::setGrabSensitivity(int grabSensitivity)
{
    // do not allow arbitrary small grab sensitivity
    if (grabSensitivity < 5)
        grabSensitivity = 5;
    setResource(GrabSensitivity, PkVariant(grabSensitivity));
}

int KoDocumentResourceManager::grabSensitivity() const
{
    if (hasResource(GrabSensitivity))
        return intResource(GrabSensitivity);
    return 5; // default value (and is used just about everywhere)
}

void KoDocumentResourceManager::setUndoStack(KUndo2Stack *undoStack)
{
    PkVariant variant;
    variant.setValue<void*>(undoStack);
    setResource(UndoStack, variant);
}

qreal KoDocumentResourceManager::documentResolution() const
{
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(hasResource(DocumentResolution), 72.0);
    return resource(DocumentResolution).toReal();
}

PkRectF KoDocumentResourceManager::documentRectInPixels() const
{
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(hasResource(DocumentRectInPixels), PkRectF(0,0, 777, 666));
    return resource(DocumentRectInPixels).toRectF();
}
