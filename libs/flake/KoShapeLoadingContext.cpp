/* This file is part of the KDE project
   SPDX-FileCopyrightText: 2007-2009, 2011 Thorsten Zachmann <zachmann@kde.org>
   SPDX-FileCopyrightText: 2007 Jan Hambrecht <jaham@gmx.net>
   SPDX-FileCopyrightText: 2014-2015 Denis Kuplyakov <dener.kup@gmail.com>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <PkMessageLogger.h>
#include <PkVariant.h>

#include <cassert>
#include <map>

#include "KoShapeLoadingContext.h"
#include "KoShape.h"
#include "KoSharedLoadingData.h"
#include "KoShapeControllerBase.h"
#include "KoMarkerCollection.h"
#include "KoDocumentResourceManager.h"
#include "KoLoadingShapeUpdater.h"

static PkSet<KoShapeLoadingContext::AdditionalAttributeData> s_additionalAttributes;

class KoShapeLoadingContext::Private
{
public:
    Private(KoStore *store, KoDocumentResourceManager *resourceManager)
            : store(store)
            , zIndex(0)
            , documentResources(resourceManager)
            , sectionModel(0)
    {
    }

    ~Private() {
        for (KoSharedLoadingData *data : sharedData) {
            delete data;
        }
    }

    KoStore *store;

    PkMap<PkString, KoShapeLayer*> layers;
    PkMap<PkString, KoShape*> drawIds;
    PkMap<PkString, std::pair<KoShape *, PkVariant> > subIds;
    PkMap<PkString, KoSharedLoadingData *> sharedData; //FIXME: use PkScopedPointer here to auto delete in destructor
    int zIndex;
    std::multimap<PkString, KoLoadingShapeUpdater*> updaterById;
    std::multimap<KoShape *, KoLoadingShapeUpdater*> updaterByShape;
    KoDocumentResourceManager *documentResources;
    KoSectionModel *sectionModel; };

KoShapeLoadingContext::KoShapeLoadingContext(KoStore *store, KoDocumentResourceManager *documentResources)
        : d(new Private(store, documentResources))
{
}

KoShapeLoadingContext::~KoShapeLoadingContext()
{
    delete d;
}

KoStore *KoShapeLoadingContext::store() const
{
    return d->store;
}

PkString KoShapeLoadingContext::mimeTypeForPath(const PkString &href, bool b)
{
    (void)href;
    (void)b;
    return "image/svg+xml";
}

KoShapeLayer * KoShapeLoadingContext::layer(const PkString & layerName)
{
    return d->layers.value(layerName, 0);
}

void KoShapeLoadingContext::addLayer(KoShapeLayer * layer, const PkString & layerName)
{
    d->layers[ layerName ] = layer;
}

void KoShapeLoadingContext::clearLayers()
{
    d->layers.clear();
}

void KoShapeLoadingContext::addShapeId(KoShape * shape, const PkString & id)
{
    d->drawIds.insert(id, shape);
    auto it = d->updaterById.lower_bound(id);
    const auto end = d->updaterById.upper_bound(id);
    while (it != end) {
        d->updaterByShape.emplace(shape, it->second);
        it = d->updaterById.erase(it);
    }
}

KoShape * KoShapeLoadingContext::shapeById(const PkString &id)
{
    return d->drawIds.value(id, 0);
}

void KoShapeLoadingContext::addShapeSubItemId(KoShape *shape, const PkVariant &subItem, const PkString &id)
{
    d->subIds.insert(id, std::pair<KoShape *, PkVariant>(shape, subItem));
}

std::pair<KoShape *, PkVariant> KoShapeLoadingContext::shapeSubItemById(const PkString &id)
{
    return d->subIds.value(id);
}


// TODO make sure to remove the shape from the loading context when loading for it failed and it was deleted. This can also happen when the parent is deleted
void KoShapeLoadingContext::updateShape(const PkString & id, KoLoadingShapeUpdater * shapeUpdater)
{
    d->updaterById.emplace(id, shapeUpdater);
}

void KoShapeLoadingContext::shapeLoaded(KoShape * shape)
{
    auto it = d->updaterByShape.lower_bound(shape);
    const auto end = d->updaterByShape.upper_bound(shape);
    while (it != end) {
        it->second->update(shape);
        delete it->second;
        it = d->updaterByShape.erase(it);
    }
}


int KoShapeLoadingContext::zIndex()
{
    return d->zIndex++;
}

void KoShapeLoadingContext::setZIndex(int index)
{
    d->zIndex = index;
}

void KoShapeLoadingContext::addSharedData(const PkString & id, KoSharedLoadingData * data)
{
    auto it(d->sharedData.find(id));
    // data will not be overwritten
    if (it == d->sharedData.end()) {
        d->sharedData.insert(id, data);
    } else {
        PkMessageLogger(__FILE__, __LINE__, __func__).warning()
            << "The id" << id << "is already registered. Data not inserted";
        assert(it == d->sharedData.end());
    }
}

KoSharedLoadingData * KoShapeLoadingContext::sharedData(const PkString & id) const
{
    KoSharedLoadingData * data = 0;
    PkMap<PkString, KoSharedLoadingData*>::const_iterator it(d->sharedData.find(id));
    if (it != d->sharedData.constEnd()) {
        data = it.value();
    }
    return data;
}

void KoShapeLoadingContext::addAdditionalAttributeData(const AdditionalAttributeData & attributeData)
{
    s_additionalAttributes.insert(attributeData);
}

PkSet<KoShapeLoadingContext::AdditionalAttributeData> KoShapeLoadingContext::additionalAttributeData()
{
    return s_additionalAttributes;
}

KoDocumentResourceManager *KoShapeLoadingContext::documentResourceManager() const
{
    return d->documentResources;
}

KoSectionModel *KoShapeLoadingContext::sectionModel()
{
    return d->sectionModel;
}

void KoShapeLoadingContext::setSectionModel(KoSectionModel *sectionModel)
{
    d->sectionModel = sectionModel;
}
