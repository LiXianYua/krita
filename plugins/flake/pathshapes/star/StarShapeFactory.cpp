/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006-2007 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "star/StarShapeFactory.h"
#include "star/StarShape.h"

#include <KoShapeFactoryBase.h>
#include <KoShapeStroke.h>
#include <KoProperties.h>
#include <KoXmlNS.h>
#include <KoColorBackground.h>
#include <KoShapeLoadingContext.h>
#include <PkColor.h>
#include <PkSharedPointer.h>
#include <PkStringList.h>


StarShapeFactory::StarShapeFactory()
    : KoShapeFactoryBase(StarShapeId, "A star shape")
{
    setToolTip("A star");
    PkStringList elementNames;
    elementNames << "regular-polygon" << "custom-shape";
    setXmlElementNames(KoXmlNS::draw, elementNames);
    setLoadingPriority(5);

    KoShapeTemplate t;
    t.id = KoPathShapeId;
    t.templateId = "star";
    t.name = "Star";
    t.family = "geometric";
    t.toolTip = "A star";
    t.iconName = "star-shape";
    KoProperties *props = new KoProperties();
    props->setProperty("corners", 5);
    PkVariant v;
    v.setValue(PkColor(255, 255, 0));
    props->setProperty("background", v);
    t.properties = props;
    addTemplate(t);

    t.id = KoPathShapeId;
    t.templateId = "flower";
    t.name = "Flower";
    t.family = "funny";
    t.toolTip = "A flower";
    t.iconName = "flower-shape";
    props = new KoProperties();
    props->setProperty("corners", 5);
    props->setProperty("baseRadius", 10.0);
    props->setProperty("tipRadius", 50.0);
    props->setProperty("baseRoundness", 0.0);
    props->setProperty("tipRoundness", 40.0);
    v.setValue(PkColor(255, 0, 255));
    props->setProperty("background", v);
    t.properties = props;
    addTemplate(t);

    t.id = KoPathShapeId;
    t.templateId = "pentagon";
    t.name = "Pentagon";
    t.family = "geometric";
    t.toolTip = "A pentagon";
    t.iconName = "pentagon-shape";
    props = new KoProperties();
    props->setProperty("corners", 5);
    props->setProperty("convex", true);
    props->setProperty("tipRadius", 50.0);
    props->setProperty("tipRoundness", 0.0);
    v.setValue(PkColor(0, 0, 255));
    props->setProperty("background", v);
    t.properties = props;
    addTemplate(t);

    t.id = KoPathShapeId;
    t.templateId = "hexagon";
    t.name = "Hexagon";
    t.family = "geometric";
    t.toolTip = "A hexagon";
    t.iconName = "hexagon-shape";
    props = new KoProperties();
    props->setProperty("corners", 6);
    props->setProperty("convex", true);
    props->setProperty("tipRadius", 50.0);
    props->setProperty("tipRoundness", 0.0);
    v.setValue(PkColor(0, 0, 255));
    props->setProperty("background", v);
    t.properties = props;
    addTemplate(t);
}

KoShape *StarShapeFactory::createDefaultShape(KoDocumentResourceManager *) const
{
    StarShape *star = new StarShape();

    star->setStroke(PkSharedPointer<KoShapeStroke>(new KoShapeStroke(1.0)));
    star->setShapeId(KoPathShapeId);

    return star;
}

KoShape *StarShapeFactory::createShape(const KoProperties *params, KoDocumentResourceManager *) const
{
    StarShape *star = new StarShape();
    if (!star) {
        return 0;
    }

    star->setCornerCount(params->intProperty("corners", 5));
    star->setConvex(params->boolProperty("convex", false));
    star->setBaseRadius(params->doubleProperty("baseRadius", 25.0));
    star->setTipRadius(params->doubleProperty("tipRadius", 50.0));
    star->setBaseRoundness(params->doubleProperty("baseRoundness", 0.0));
    star->setTipRoundness(params->doubleProperty("tipRoundness", 0.0));
    star->setStroke(PkSharedPointer<KoShapeStroke>(new KoShapeStroke(1.0)));
    star->setShapeId(KoPathShapeId);
    PkVariant v;
    if (params->property("background", v)) {
        star->setBackground(PkSharedPointer<KoColorBackground>(new KoColorBackground(v.value<PkColor>())));
    }

    return star;
}

bool StarShapeFactory::supports(const PkXmlElement &e, KoShapeLoadingContext &context) const
{
    (void)context;
    if (e.localName() == "regular-polygon" && e.namespaceURI() == KoXmlNS::draw) {
        return true;
    }
    return (e.localName() == "custom-shape" && e.namespaceURI() == KoXmlNS::draw
            && e.attributeNS(KoXmlNS::draw, "engine", "") == "calligra:star");
}
