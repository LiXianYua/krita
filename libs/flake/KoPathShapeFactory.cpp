/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2011 Thorsten Zachmann <zachmann@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include <QtCore/QtCore>
#include <PkFlakeBridge.h>
#include "KoPathShapeFactory.h"
#include "KoPathShape.h"
#include "KoShapeStroke.h"
#include "KoMarkerCollection.h"
#include "KoDocumentResourceManager.h"
#include "KoShapeLoadingContext.h"
#include "KoInsets.h"

#include <klocalizedstring.h>

#include <KoXmlNS.h>

#include "kis_pointer_utils.h"

KoPathShapeFactory::KoPathShapeFactory(const PkStringList&)
        : KoShapeFactoryBase(KoPathShapeId, toPkString(i18n("Simple path shape")))
{
    setToolTip(toPkString(i18n("A simple path shape")));
    setIconName("pathshape");
    PkStringList elementNames;
    elementNames << "path" << "line" << "polyline" << "polygon";
    setXmlElementNames(KoXmlNS::draw, elementNames);
    setLoadingPriority(0);
}

KoShape *KoPathShapeFactory::createDefaultShape(KoDocumentResourceManager *) const
{
    KoPathShape* path = new KoPathShape();
    path->moveTo(PkPointF(0, 50));
    path->curveTo(PkPointF(0, 120), PkPointF(50, 120), PkPointF(50, 50));
    path->curveTo(PkPointF(50, -20), PkPointF(100, -20), PkPointF(100, 50));
    path->normalize();
    path->setStroke(PkSharedPointer<KoShapeStroke>(new KoShapeStroke(1.0)));
    return path;
}

bool KoPathShapeFactory::supports(const PkXmlElement & e, KoShapeLoadingContext &context) const
{
    Q_UNUSED(context);
    if (e.namespaceURI() == toQString(KoXmlNS::draw)) {
        if (e.localName() == "path")
            return true;
        if (e.localName() == "line")
            return true;
        if (e.localName() == "polyline")
            return true;
        if (e.localName() == "polygon")
            return true;
    }

    return false;
}

void KoPathShapeFactory::newDocumentResourceManager(KoDocumentResourceManager *manager) const
{
    // we also need a MarkerCollection so add if it is not there yet
    if (!manager->hasResource(KoDocumentResourceManager::MarkerCollection)) {
        KoMarkerCollection *markerCollection = new KoMarkerCollection(manager);
        manager->setResource(KoDocumentResourceManager::MarkerCollection, PkVariant::fromValue(markerCollection));
    }
}
