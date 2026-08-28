/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006 Thorsten Zachmann <zachmann@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "EllipseShapeFactory.h"
#include "EllipseShape.h"
#include <KoShapeStroke.h>
#include <KoXmlNS.h>
#include <KoShapeLoadingContext.h>
#include <PkSharedPointer.h>
#include <PkStringList.h>

#include <utility>

EllipseShapeFactory::EllipseShapeFactory()
    : KoShapeFactoryBase(EllipseShapeId, "Ellipse")
{
    setToolTip("An ellipse");
    setFamily("geometric");
    setLoadingPriority(1);

    PkList<std::pair<PkString, PkStringList> > elementNamesList;
    elementNamesList.append(std::make_pair(PkString(KoXmlNS::draw), PkStringList{"circle"}));
    elementNamesList.append(std::make_pair(PkString(KoXmlNS::draw), PkStringList{"ellipse"}));
    elementNamesList.append(std::make_pair(PkString(KoXmlNS::svg), PkStringList{"circle"}));
    elementNamesList.append(std::make_pair(PkString(KoXmlNS::svg), PkStringList{"ellipse"}));
    elementNamesList.append(std::make_pair(PkString(KoXmlNS::svg), PkStringList{"sodipodi:arc"}));
    elementNamesList.append(std::make_pair(PkString(KoXmlNS::svg), PkStringList{"krita:arc"}));
    setXmlElements(elementNamesList);
}

KoShape *EllipseShapeFactory::createDefaultShape(KoDocumentResourceManager *) const
{
    EllipseShape *ellipse = new EllipseShape();

    ellipse->setStroke(PkSharedPointer<KoShapeStroke>(new KoShapeStroke(1.0)));
    ellipse->setShapeId(KoPathShapeId);

    // S-09/M5 GAP: the default gradient is renderer-only visualization.

    return ellipse;
}

bool EllipseShapeFactory::supports(const PkXmlElement &e, KoShapeLoadingContext &context) const
{
    (void)context;
    return (e.localName() == "ellipse" || e.localName() == "circle")
           && e.namespaceURI() == KoXmlNS::draw;
}
