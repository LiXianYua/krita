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
#include <QImage>
#include <QTransform>
#include <KoShapeLoadingContext.h>
#include <KoProperties.h>

// KDE
#include <klocalizedstring.h>

ImageShapeFactory::ImageShapeFactory()
    : KoShapeFactoryBase(ImageShapeId, i18n("Image shape"))
{
    setToolTip(i18n("A shape that shows an image (PNG/JPG/TIFF)"));

    QList<QPair<QString, QStringList> > elementNamesList;
    elementNamesList.append(qMakePair(toQString(KoXmlNS::draw), QStringList("image")));
    elementNamesList.append(qMakePair(toQString(KoXmlNS::svg), QStringList("image")));
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
    if (var.canConvert<QImage>()) {
        shape->setImage(var.value<QImage>());
    }

    var = params->value("viewboxTransform");
    if (var.canConvert<QTransform>()) {
        shape->setViewBoxTransform(var.value<QTransform>());
    }
    return shape;
}

bool ImageShapeFactory::supports(const QDomElement &e, KoShapeLoadingContext &context) const
{
    Q_UNUSED(context);
    return e.localName() == "image" &&
            (e.namespaceURI() == toQString(KoXmlNS::draw) || e.namespaceURI() == toQString(KoXmlNS::svg));
}
