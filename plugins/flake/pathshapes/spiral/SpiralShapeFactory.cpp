/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2007 Rob Buis <buis@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "SpiralShapeFactory.h"
#include "SpiralShape.h"
#include <KoShapeStroke.h>
#include <KoShapeLoadingContext.h>
#include <PkSharedPointer.h>

SpiralShapeFactory::SpiralShapeFactory()
    : KoShapeFactoryBase(SpiralShapeId, "Spiral")
{
    setToolTip("A spiral shape");
    setFamily("geometric");
    setLoadingPriority(1);
}

KoShape *SpiralShapeFactory::createDefaultShape(KoDocumentResourceManager *) const
{
    SpiralShape *spiral = new SpiralShape();

    spiral->setStroke(PkSharedPointer<KoShapeStroke>(new KoShapeStroke(1.0)));
    spiral->setShapeId(KoPathShapeId);

    return spiral;
}

bool SpiralShapeFactory::supports(const PkXmlElement &e, KoShapeLoadingContext &context) const
{
    (void)e;
    (void)context;
    return false;
}
