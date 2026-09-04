/* This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2009 Inge Wallin <inge@lysator.liu.se>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QtCore/QtCore>
#include <PkFlakeBridge.h>

// Own
#include "ImageShapeFactory.h"

// ImageShape
#include "ImageShape.h"

// Calligra
#include <KoXmlNS.h>
#include <PkImage.h>
#include <PkTransform.h>
#include <KoShapeLoadingContext.h>
#include <KoProperties.h>

// KDE
#include <klocalizedstring.h>

ImageShapeFactory::ImageShapeFactory()
    : KoShapeFactoryBase(ImageShapeId, i18n("Image shape"))
{
    setToolTip(i18n("A shape that shows an image (PNG/JPG/TIFF)"));

    PkList<std::pair<PkString, PkStringList> > elementNamesList;
    elementNamesList.append(qMakePair(toQString(KoXmlNS::draw), PkStringList("image")));
    elementNamesList.append(qMakePair(toQString(KoXmlNS::svg), PkStringList("image")));
    setXmlElements(elementNamesList);
    setLoadingPriority(1);
}

KoShape *ImageShapeFactory::createDefaultShape(KoDocumentResourceManager */*documentResources*/) const
{
    ImageShape *shape = new ImageShape();
    shape->setShapeId(ImageShapeId);

    return shape;
}


KoShape *ImageShapeFactory::createShape(const KoProperties *params, KoDocumentResourceManager */*documentResources*/) const
{
    ImageShape *shape = new ImageShape();
    shape->setShapeId(ImageShapeId);

    PkVariant var = params->value("image");
    if (var.canConvert<PkImage>()) {
        shape->setImage(var.value<PkImage>());
    }

    var = params->value("viewboxTransform");
    if (var.canConvert<PkTransform>()) {
        shape->setViewBoxTransform(var.value<PkTransform>());
    }
    return shape;
}

bool ImageShapeFactory::supports(const PkXmlElement &e, KoShapeLoadingContext &context) const
{
    Q_UNUSED(context);
    return e.localName() == "image" &&
            (e.namespaceURI() == toQString(KoXmlNS::draw) || e.namespaceURI() == toQString(KoXmlNS::svg));
}
