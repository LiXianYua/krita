/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006 Thomas Zander <zander@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QtCore/QtCore>
#include <PkFlakeBridge.h>

#include "RectangleShapeFactory.h"
#include "RectangleShape.h"
#include <QLinearGradient>
#include "KoShapeStroke.h"
#include <KoXmlNS.h>
#include <KoGradientBackground.h>
#include <KoShapeLoadingContext.h>
#include <KoProperties.h>
#include "kis_assert.h"

#include <klocalizedstring.h>

RectangleShapeFactory::RectangleShapeFactory()
    : KoShapeFactoryBase(RectangleShapeId, i18n("Rectangle"))
{
    setToolTip(i18n("A rectangle"));
    setFamily("geometric");
    setLoadingPriority(1);

    PkList<std::pair<PkString, PkStringList> > elementNamesList;
    elementNamesList.append(qMakePair(toQString(KoXmlNS::draw), PkStringList("rect")));
    elementNamesList.append(qMakePair(toQString(KoXmlNS::svg), PkStringList("rect")));
    setXmlElements(elementNamesList);
}

KoShape *RectangleShapeFactory::createDefaultShape(KoDocumentResourceManager *) const
{
    RectangleShape *rect = new RectangleShape();

    rect->setStroke(PkSharedPointer<KoShapeStroke>(new KoShapeStroke(1.0)));
    rect->setShapeId(KoPathShapeId);

    QLinearGradient *gradient = new QLinearGradient(PkPointF(0, 0), PkPointF(1, 1));
    gradient->setCoordinateMode(PkGradient::ObjectBoundingMode);

    gradient->setColorAt(0.0, Qt::white);
    gradient->setColorAt(1.0, Qt::green);
    rect->setBackground(PkSharedPointer<KoGradientBackground>(new KoGradientBackground(gradient)));

    return rect;
}

KoShape *RectangleShapeFactory::createShape(const KoProperties *params, KoDocumentResourceManager *documentResources) const
{
    KoShape *shape = createDefaultShape(documentResources);
    RectangleShape *rectShape = dynamic_cast<RectangleShape*>(shape);
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(rectShape, shape);

    rectShape->setSize(
        PkSizeF(params->doubleProperty("width", rectShape->size().width()),
               params->doubleProperty("height", rectShape->size().height())));

    rectShape->setAbsolutePosition(
        PkPointF(params->doubleProperty("x", rectShape->absolutePosition(KoFlake::TopLeft).x()),
                params->doubleProperty("y", rectShape->absolutePosition(KoFlake::TopLeft).y())),
        KoFlake::TopLeft);


    rectShape->setCornerRadiusX(params->doubleProperty("rx", 0.0));
    rectShape->setCornerRadiusY(params->doubleProperty("ry", 0.0));

    return shape;
}

bool RectangleShapeFactory::supports(const PkXmlElement &e, KoShapeLoadingContext &/*context*/) const
{
    Q_UNUSED(e);
    return (e.localName() == "rect" && e.namespaceURI() == toQString(KoXmlNS::draw));
}
