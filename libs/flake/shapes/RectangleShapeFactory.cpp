/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006 Thomas Zander <zander@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <PkFlakeBridge.h>
#include <KoGradientBridge.h>
#include <PkGradient.h>

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
    : KoShapeFactoryBase(RectangleShapeId, toPkString(i18n("Rectangle")))
{
    setToolTip(toPkString(i18n("A rectangle")));
    setFamily("geometric");
    setLoadingPriority(1);

    PkList<std::pair<PkString, PkStringList> > elementNamesList;
    elementNamesList.append(std::make_pair(PkString(KoXmlNS::draw), PkStringList{PkString("rect")}));
    elementNamesList.append(std::make_pair(PkString(KoXmlNS::svg), PkStringList{PkString("rect")}));
    setXmlElements(elementNamesList);
}

KoShape *RectangleShapeFactory::createDefaultShape(KoDocumentResourceManager *) const
{
    RectangleShape *rect = new RectangleShape();

    rect->setStroke(PkSharedPointer<KoShapeStroke>(new KoShapeStroke(1.0)));
    rect->setShapeId(KoPathShapeId);

    PkGradient *gradient = new PkGradient(PkGradient::linear(PkPointF(0, 0), PkPointF(1, 1)));
    gradient->setCoordinateMode(PkGradientEnums::ObjectBoundingMode);

    gradient->setColorAt(0.0, PkColor(Pk::white));
    gradient->setColorAt(1.0, PkColor(Pk::green));
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
    return (toQString(e.localName()) == "rect" && toQString(e.namespaceURI()) == toQString(KoXmlNS::draw));
}
