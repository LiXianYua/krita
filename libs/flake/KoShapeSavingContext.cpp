/* This file is part of the KDE project
   SPDX-FileCopyrightText: 2004-2006 David Faure <faure@kde.org>
   SPDX-FileCopyrightText: 2007-2009, 2011 Thorsten Zachmann <zachmann@kde.org>
   SPDX-FileCopyrightText: 2007 Jan Hambrecht <jaham@gmx.net>
   SPDX-FileCopyrightText: 2010 Benjamin Port <port.benjamin@gmail.com>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <PkHash.h>
#include <PkMessageLogger.h>
#include "KoShapeSavingContext.h"

#include <cassert>

#include "KoShapeLayer.h"
#include "KoMarker.h"

#include <KoXmlWriter.h>
#include <KoStore.h>
#include <KoStoreDevice.h>
#include <KoSharedSavingData.h>

#include <pk/uuid/PkNodeId.h>
#include <PkImage.h>
#include <KisMimeDatabase.h>

class KoShapeSavingContextPrivate {
public:
    KoShapeSavingContextPrivate(KoXmlWriter&);
    ~KoShapeSavingContextPrivate();

    KoXmlWriter *xmlWriter;
    KoShapeSavingContext::ShapeSavingOptions savingOptions;

    PkList<const KoShapeLayer*> layers;
    PkMap<PkString, KoSharedSavingData*> sharedData;

    PkMap<qint64, PkString> imageNames;
    int imageId;
    PkMap<PkString, PkImage> images;

    PkHash<const KoShape *, PkTransform> shapeOffsets;
    PkMap<const KoMarker *, PkString> markerRefs;

    PkMap<PkString, int> referenceCounters;
    PkMap<PkString, PkList<const void*> > prefixedReferences;

};

KoShapeSavingContextPrivate::KoShapeSavingContextPrivate(KoXmlWriter &w)
    : xmlWriter(&w)
    , imageId(0)
{
}

KoShapeSavingContextPrivate::~KoShapeSavingContextPrivate()
{
    for (KoSharedSavingData *data : sharedData) {
        delete data;
    }
}

KoShapeSavingContext::KoShapeSavingContext(KoXmlWriter &xmlWriter)
    : d(new KoShapeSavingContextPrivate(xmlWriter))
{
    // by default allow saving of draw:id + xml:id
    addOption(KoShapeSavingContext::DrawId);
}

KoShapeSavingContext::~KoShapeSavingContext()
{
    delete d;
}

KoXmlWriter & KoShapeSavingContext::xmlWriter()
{
    return *d->xmlWriter;
}

void KoShapeSavingContext::setXmlWriter(KoXmlWriter &xmlWriter)
{
    d->xmlWriter = &xmlWriter;
}

bool KoShapeSavingContext::isSet(ShapeSavingOption option) const
{
    return d->savingOptions & option;
}

void KoShapeSavingContext::setOptions(ShapeSavingOptions options)
{
    d->savingOptions = options;
}

KoShapeSavingContext::ShapeSavingOptions KoShapeSavingContext::options() const
{
    return d->savingOptions;
}

void KoShapeSavingContext::addOption(ShapeSavingOption option)
{
    d->savingOptions = d->savingOptions | option;
}

void KoShapeSavingContext::removeOption(ShapeSavingOption option)
{
    if (isSet(option))
        d->savingOptions = d->savingOptions ^ option; // xor to remove it.
}

void KoShapeSavingContext::addLayerForSaving(const KoShapeLayer *layer)
{
    if (layer && ! d->layers.contains(layer))
        d->layers.append(layer);
}

void KoShapeSavingContext::saveLayerSet(KoXmlWriter &xmlWriter) const
{
    xmlWriter.startElement("draw:layer-set");
    for (const KoShapeLayer *layer : d->layers) {
        xmlWriter.startElement("draw:layer");
        xmlWriter.addAttribute("draw:name", layer->name());
        if (layer->isGeometryProtected())
            xmlWriter.addAttribute("draw:protected", "true");
        if (! layer->isVisible(false))
            xmlWriter.addAttribute("draw:display", "none");
        xmlWriter.endElement();  // draw:layer
    }
    xmlWriter.endElement();  // draw:layer-set
}

void KoShapeSavingContext::clearLayers()
{
    d->layers.clear();
}

PkMap<qint64, PkString> KoShapeSavingContext::imagesToSave()
{
    return d->imageNames;
}

PkString KoShapeSavingContext::markerRef(const KoMarker */*marker*/)
{
    return PkString();
}

void KoShapeSavingContext::addSharedData(const PkString &id, KoSharedSavingData * data)
{
    PkMap<PkString, KoSharedSavingData*>::iterator it(d->sharedData.find(id));
    // data will not be overwritten
    if (it == d->sharedData.end()) {
        d->sharedData.insert(id, data);
    } else {
        PkMessageLogger(__FILE__, __LINE__, __func__).warning()
            << "The id" << id << "is already registered. Data not inserted";
        assert(it == d->sharedData.end());
    }
}

KoSharedSavingData * KoShapeSavingContext::sharedData(const PkString &id) const
{
    KoSharedSavingData * data = 0;
    PkMap<PkString, KoSharedSavingData*>::const_iterator it(d->sharedData.constFind(id));
    if (it != d->sharedData.constEnd()) {
        data = it.value();
    }
    return data;
}

void KoShapeSavingContext::addShapeOffset(const KoShape *shape, const PkTransform &m)
{
    d->shapeOffsets.insert(shape, m);
}

void KoShapeSavingContext::removeShapeOffset(const KoShape *shape)
{
    d->shapeOffsets.remove(shape);
}

PkTransform KoShapeSavingContext::shapeOffset(const KoShape *shape) const
{
    return d->shapeOffsets.value(shape, PkTransform());
}
