/* This file is part of the KDE project
   SPDX-FileCopyrightText: 2007-2009, 2011 Thorsten Zachmann <zachmann@kde.org>
   SPDX-FileCopyrightText: 2007 Jan Hambrecht <jaham@gmx.net>
   SPDX-FileCopyrightText: 2014-2015 Denis Kuplyakov <dener.kup@gmail.com>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QMultiMap>
#include <PkVariant.h>

#include "KoShapeLoadingContext.h"
#include "KoShape.h"
#include "KoSharedLoadingData.h"
#include "KoShapeControllerBase.h"
#include "KoMarkerCollection.h"
#include "KoDocumentResourceManager.h"
#include "KoLoadingShapeUpdater.h"

#include <FlakeDebug.h>

uint qHash(const KoShapeLoadingContext::AdditionalAttributeData & attributeData)
{
    return qHash(toQString(attributeData.name));
}

static PkSet<KoShapeLoadingContext::AdditionalAttributeData> s_additionalAttributes;

class Q_DECL_HIDDEN KoShapeLoadingContext::Private
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
        Q_FOREACH (KoSharedLoadingData * data, sharedData) {
            delete data;
        }
    }

    KoStore *store;

    PkMap<PkString, KoShapeLayer*> layers;
    PkMap<PkString, KoShape*> drawIds;
    PkMap<PkString, std::pair<KoShape *, PkVariant> > subIds;
    PkMap<PkString, KoSharedLoadingData *> sharedData; //FIXME: use PkScopedPointer here to auto delete in destructor
    int zIndex;
    QMultiMap<PkString, KoLoadingShapeUpdater*> updaterById;
    QMultiMap<KoShape *, KoLoadingShapeUpdater*> updaterByShape;
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
    Q_UNUSED(href);
    Q_UNUSED(b);
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
    auto it(d->updaterById.find(id));
    while (it != d->updaterById.end() && it.key() == id) {
        d->updaterByShape.insert(shape, it.value());
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
    d->updaterById.insert(id, shapeUpdater);
}

void KoShapeLoadingContext::shapeLoaded(KoShape * shape)
{
    auto it(d->updaterByShape.find(shape));
    while (it != d->updaterByShape.end() && it.key() == shape) {
        it.value()->update(shape);
        delete it.value();
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
        warnFlake << "The id" << id << "is already registered. Data not inserted";
        Q_ASSERT(it == d->sharedData.end());
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
