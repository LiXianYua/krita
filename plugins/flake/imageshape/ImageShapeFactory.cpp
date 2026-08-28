/* This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2009 Inge Wallin <inge@lysator.liu.se>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

// Own
#include "ImageShapeFactory.h"

// ImageShape
#include "ImageShape.h"
//#include "ImageShapeConfigWidget.h"

// Calligra
#include <KoXmlNS.h>
#include <PkImage.h>
#include <PkStringList.h>
#include <PkTransform.h>
#include <KoShapeLoadingContext.h>
#include <KoProperties.h>
#include <kis_assert.h>

#include <utility>

ImageShapeFactory::ImageShapeFactory()
    : KoShapeFactoryBase(ImageShapeId, "Image shape")
{
    setToolTip("A shape that shows an image (PNG/JPG/TIFF)");

    PkList<std::pair<PkString, PkStringList> > elementNamesList;
    elementNamesList.append(std::make_pair(PkString(KoXmlNS::draw), PkStringList{"image"}));
    elementNamesList.append(std::make_pair(PkString(KoXmlNS::svg), PkStringList{"image"}));
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

    KIS_SAFE_ASSERT_RECOVER(var.canConvert<PkImage>());
    shape->setImage(var.value<PkImage>());

    var = params->value("viewboxTransform");
    if (var.canConvert<PkTransform>()) {
        shape->setViewBoxTransform(var.value<PkTransform>());
    }
    return shape;
}

bool ImageShapeFactory::supports(const PkXmlElement &e, KoShapeLoadingContext &context) const
{
    (void)context;
    return e.localName() == "image" &&
            (e.namespaceURI() == KoXmlNS::draw || e.namespaceURI() == KoXmlNS::svg);
}
