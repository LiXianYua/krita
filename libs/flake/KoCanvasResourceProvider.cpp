/*
   SPDX-FileCopyrightText: 2006 Boudewijn Rempt (boud@valdyas.org)
   SPDX-FileCopyrightText: 2007, 2010 Thomas Zander <zander@kde.org>
   SPDX-FileCopyrightText: 2008 Carlos Licea <carlos.licea@kdemail.net>
   SPDX-FileCopyrightText: 2011 Jan Hambrecht <jaham@gmx.net>

   SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include "KoCanvasResourceProvider.h"

#include <PkVariant.h>
#include <FlakeDebug.h>

#include "KoShape.h"
#include "KoShapeStroke.h"
#include "KoResourceManager_p.h"
#include <KoColorSpaceRegistry.h>

#include <KoCanvasResourcesInterface.h>

struct CanvasResourceProviderInterfaceWrapper : public KoCanvasResourcesInterface
{
    CanvasResourceProviderInterfaceWrapper(KoCanvasResourceProvider *provider)
        : m_provider(provider)
    {
    }

    PkVariant resource(int key) const override {
        return m_provider->resource(key);
    }

private:
    KoCanvasResourceProvider *m_provider = 0;
};


class KoCanvasResourceProvider::Private
{
public:
    Private(KoCanvasResourceProvider *q)
        : interfaceWrapper(new CanvasResourceProviderInterfaceWrapper(q))
    {
    }

    KoResourceManager manager;
    PkSharedPointer<CanvasResourceProviderInterfaceWrapper> interfaceWrapper;
};

KoCanvasResourceProvider::KoCanvasResourceProvider(PkObject *parent)
    : PkObject(parent)
    , d(new Private(this))
{
    const KoColorSpace* cs = KoColorSpaceRegistry::instance()->rgb8();
    setForegroundColor(KoColor(Pk::black, cs));
    setBackgroundColor(KoColor(PkColor(Pk::white), cs));

    PkObject::connect(&d->manager, &KoResourceManager::resourceChanged,
            this, &KoCanvasResourceProvider::canvasResourceChanged);
    PkObject::connect(&d->manager, &KoResourceManager::resourceChangeAttempted,
            this, &KoCanvasResourceProvider::canvasResourceChangeAttempted);
}

KoCanvasResourceProvider::~KoCanvasResourceProvider()
{
    delete d;
}

void KoCanvasResourceProvider::canvasResourceChanged(int key, const PkVariant &value)
{
    activateSignal<int, const PkVariant &>(
        this, PkMemberFnKey::from(&KoCanvasResourceProvider::canvasResourceChanged), key, value);
}

void KoCanvasResourceProvider::canvasResourceChangeAttempted(int key, const PkVariant &value)
{
    activateSignal<int, const PkVariant &>(
        this, PkMemberFnKey::from(&KoCanvasResourceProvider::canvasResourceChangeAttempted), key, value);
}

void KoCanvasResourceProvider::setResource(int key, const PkVariant &value)
{
    d->manager.setResource(key, value);
}

PkVariant KoCanvasResourceProvider::resource(int key) const
{
    return d->manager.resource(key);
}

void KoCanvasResourceProvider::setResource(int key, const KoColor &color)
{
    PkVariant v;
    v.setValue(color);
    setResource(key, v);
}

void KoCanvasResourceProvider::setResource(int key, KoShape *shape)
{
    PkVariant v;
    v.setValue(shape);
    setResource(key, v);
}

void KoCanvasResourceProvider::setResource(int key, const KoUnit &unit)
{
    PkVariant v;
    v.setValue(unit);
    setResource(key, v);
}

KoColor KoCanvasResourceProvider::koColorResource(int key) const
{
    return d->manager.koColorResource(key);
}

void KoCanvasResourceProvider::setForegroundColor(const KoColor &color)
{
    setResource(KoCanvasResource::ForegroundColor, color);
}

KoColor KoCanvasResourceProvider::foregroundColor() const
{
    return koColorResource(KoCanvasResource::ForegroundColor);
}

void KoCanvasResourceProvider::setBackgroundColor(const KoColor &color)
{
    setResource(KoCanvasResource::BackgroundColor, color);
}

KoColor KoCanvasResourceProvider::backgroundColor() const
{
    return koColorResource(KoCanvasResource::BackgroundColor);
}

KoShape *KoCanvasResourceProvider::koShapeResource(int key) const
{
    return d->manager.koShapeResource(key);
}

KoUnit KoCanvasResourceProvider::unitResource(int key) const
{
    return resource(key).value<KoUnit>();
}

void KoCanvasResourceProvider::setHandleRadius(int handleSize)
{
    // do not allow arbitrary small handles
    if (handleSize < 5)
        handleSize = 5;
    setResource(KoCanvasResource::HandleRadius, PkVariant(handleSize));
}

int KoCanvasResourceProvider::handleRadius() const
{
    if (hasResource(KoCanvasResource::HandleRadius))
        return intResource(KoCanvasResource::HandleRadius);
    return 5; // default value (and is used just about everywhere)
}

void KoCanvasResourceProvider::setDecorationThickness(int decorationThickness)
{
    if (decorationThickness < 1)
        decorationThickness = 1;
    setResource(KoCanvasResource::DecorationThickness, PkVariant(decorationThickness));
}

int KoCanvasResourceProvider::decorationThickness() const
{
    if (hasResource(KoCanvasResource::DecorationThickness))
        return intResource(KoCanvasResource::DecorationThickness);
    return 1;
}

void KoCanvasResourceProvider::setUsingOtherColor(bool usingOtherColor)
{
    setResource(KoCanvasResource::UsingOtherColor, PkVariant(usingOtherColor));
}

bool KoCanvasResourceProvider::isUsingOtherColor() const
{
    if (hasResource(KoCanvasResource::UsingOtherColor))
        return boolResource(KoCanvasResource::UsingOtherColor);
    return false;
}

bool KoCanvasResourceProvider::boolResource(int key) const
{
    return d->manager.boolResource(key);
}

int KoCanvasResourceProvider::intResource(int key) const
{
    return d->manager.intResource(key);
}

PkString KoCanvasResourceProvider::stringResource(int key) const
{
    return d->manager.stringResource(key);
}

PkSizeF KoCanvasResourceProvider::sizeResource(int key) const
{
    return d->manager.sizeResource(key);
}

bool KoCanvasResourceProvider::hasResource(int key) const
{
    return d->manager.hasResource(key);
}

void KoCanvasResourceProvider::clearResource(int key)
{
    d->manager.clearResource(key);
}

void KoCanvasResourceProvider::addDerivedResourceConverter(KoDerivedResourceConverterSP converter)
{
    d->manager.addDerivedResourceConverter(converter);
}

bool KoCanvasResourceProvider::hasDerivedResourceConverter(int key)
{
    return d->manager.hasDerivedResourceConverter(key);
}

void KoCanvasResourceProvider::removeDerivedResourceConverter(int key)
{
    d->manager.removeDerivedResourceConverter(key);
}

bool KoCanvasResourceProvider::hasAbstractResource(int key)
{
    return d->manager.hasAbstractResource(key);
}

void KoCanvasResourceProvider::removeAbstractResource(int key)
{
    d->manager.removeAbstractResource(key);
}

void KoCanvasResourceProvider::addResourceUpdateMediator(KoResourceUpdateMediatorSP mediator)
{
    d->manager.addResourceUpdateMediator(mediator);
}

bool KoCanvasResourceProvider::hasResourceUpdateMediator(int key)
{
    return d->manager.hasResourceUpdateMediator(key);
}

void KoCanvasResourceProvider::removeResourceUpdateMediator(int key)
{
    d->manager.removeResourceUpdateMediator(key);
}

void KoCanvasResourceProvider::addActiveCanvasResourceDependency(KoActiveCanvasResourceDependencySP dep)
{
    d->manager.addActiveCanvasResourceDependency(dep);
}

bool KoCanvasResourceProvider::hasActiveCanvasResourceDependency(int sourceKey, int targetKey) const
{
    return d->manager.hasActiveCanvasResourceDependency(sourceKey, targetKey);
}

void KoCanvasResourceProvider::removeActiveCanvasResourceDependency(int sourceKey, int targetKey)
{
    d->manager.removeActiveCanvasResourceDependency(sourceKey, targetKey);
}

void KoCanvasResourceProvider::setAbstractResource(KoAbstractCanvasResourceInterfaceSP abstractResource)
{
    d->manager.setAbstractResource(abstractResource);
}

KoCanvasResourcesInterfaceSP KoCanvasResourceProvider::canvasResourcesInterface() const
{
    return d->interfaceWrapper;
}
