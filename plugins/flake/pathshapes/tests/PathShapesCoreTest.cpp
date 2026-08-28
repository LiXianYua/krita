/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "../PathShapesPlugin.h"
#include "../ellipse/EllipseShape.h"
#include "../ellipse/EllipseShapeFactory.h"
#include "../rectangle/RectangleShape.h"
#include "../rectangle/RectangleShapeFactory.h"
#include "../spiral/SpiralShape.h"
#include "../spiral/SpiralShapeFactory.h"
#include "../star/StarShape.h"
#include "../star/StarShapeFactory.h"

#include <KoShapeLoadingContext.h>
#include <KoShapeRegistry.h>
#include <KoXmlNS.h>
#include <PkXmlDocument.h>

#include <cmath>
#include <limits>
#include <memory>

namespace
{
bool closeEnough(double lhs, double rhs)
{
    return std::abs(lhs - rhs) < 1e-12;
}

int rectangleClonePreservesClampedGeometry()
{
    RectangleShape shape;
    shape.setCornerRadiusX(-12.0);
    shape.setCornerRadiusY(125.0);
    if (!closeEnough(shape.cornerRadiusX(), 0.0)) return 1;
    if (!closeEnough(shape.cornerRadiusY(), 100.0)) return 2;

    shape.setCornerRadiusX(std::numeric_limits<double>::quiet_NaN());
    shape.setCornerRadiusY(std::numeric_limits<double>::quiet_NaN());
    if (!closeEnough(shape.cornerRadiusX(), 0.0)) return 6;
    if (!closeEnough(shape.cornerRadiusY(), 0.0)) return 7;

    std::unique_ptr<KoShape> cloneBase(shape.cloneShape());
    const RectangleShape *clone = dynamic_cast<const RectangleShape *>(cloneBase.get());
    if (!clone) return 3;
    if (!closeEnough(clone->cornerRadiusX(), 0.0)) return 4;
    if (!closeEnough(clone->cornerRadiusY(), 0.0)) return 5;
    return 0;
}

int ellipseClonePreservesConfiguration()
{
    EllipseShape shape;
    shape.setType(EllipseShape::Pie);
    shape.setStartAngle(15.0);
    shape.setEndAngle(275.0);

    std::unique_ptr<KoShape> cloneBase(shape.cloneShape());
    const EllipseShape *clone = dynamic_cast<const EllipseShape *>(cloneBase.get());
    if (!clone) return 10;
    if (clone->type() != EllipseShape::Pie) return 11;
    if (!closeEnough(clone->startAngle(), 15.0)) return 12;
    if (!closeEnough(clone->endAngle(), 275.0)) return 13;
    return 0;
}

int spiralClonePreservesConfiguration()
{
    SpiralShape shape;
    shape.setType(SpiralShape::Line);
    shape.setFade(0.75);
    shape.setClockWise(false);

    std::unique_ptr<KoShape> cloneBase(shape.cloneShape());
    const SpiralShape *clone = dynamic_cast<const SpiralShape *>(cloneBase.get());
    if (!clone) return 20;
    if (clone->type() != SpiralShape::Line) return 21;
    if (!closeEnough(clone->fade(), 0.75)) return 22;
    if (clone->clockWise()) return 23;
    return 0;
}

int starClonePreservesConfiguration()
{
    StarShape shape;
    shape.setCornerCount(7);
    shape.setBaseRadius(-14.0);
    shape.setTipRadius(61.0);
    shape.setConvex(true);

    std::unique_ptr<KoShape> cloneBase(shape.cloneShape());
    const StarShape *clone = dynamic_cast<const StarShape *>(cloneBase.get());
    if (!clone) return 30;
    if (clone->cornerCount() != 7) return 31;
    if (!closeEnough(clone->baseRadius(), 14.0)) return 32;
    if (!closeEnough(clone->tipRadius(), 61.0)) return 33;
    if (!clone->convex()) return 34;
    return 0;
}

PkXmlElement namespacedElement(PkXmlDocument &document,
                               const PkString &prefix,
                               const PkString &localName,
                               const PkString &nameSpace)
{
    PkXmlElement element = document.createElement(prefix + ":" + localName);
    element.setAttribute(PkString("xmlns:") + prefix, nameSpace);
    return element;
}

int factoriesPreserveXmlSupport()
{
    KoShapeLoadingContext context(nullptr, nullptr);

    PkXmlDocument ellipseDocument;
    PkXmlElement ellipse = namespacedElement(ellipseDocument, "draw", "ellipse", KoXmlNS::draw);
    if (!EllipseShapeFactory().supports(ellipse, context)) return 40;

    PkXmlDocument rectangleDocument;
    PkXmlElement rectangle = namespacedElement(rectangleDocument, "draw", "rect", KoXmlNS::draw);
    if (!RectangleShapeFactory().supports(rectangle, context)) return 41;

    PkXmlDocument starDocument;
    PkXmlElement star = namespacedElement(starDocument, "draw", "regular-polygon", KoXmlNS::draw);
    if (!StarShapeFactory().supports(star, context)) return 42;

    PkXmlDocument customStarDocument;
    PkXmlElement customStar = namespacedElement(customStarDocument, "draw", "custom-shape", KoXmlNS::draw);
    customStar.setAttribute("draw:engine", "calligra:star");
    if (!StarShapeFactory().supports(customStar, context)) return 43;

    PkXmlDocument unsupportedDocument;
    PkXmlElement unsupported = namespacedElement(unsupportedDocument, "draw", "spiral", KoXmlNS::draw);
    if (SpiralShapeFactory().supports(unsupported, context)) return 44;
    return 0;
}

int registrationIsIdempotentAndComplete()
{
    KoShapeRegistry *registry = KoShapeRegistry::instance();
    registerPathShapes();
    const int countAfterFirstCall = registry->count();
    registerPathShapes();
    if (registry->count() != countAfterFirstCall) return 50;
    if (!registry->contains(StarShapeId)) return 51;
    if (!registry->contains(RectangleShapeId)) return 52;
    if (!registry->contains(SpiralShapeId)) return 53;
    if (!registry->contains(EllipseShapeId)) return 54;
    return 0;
}
} // namespace

int main()
{
    const int rectangleResult = rectangleClonePreservesClampedGeometry();
    if (rectangleResult) return rectangleResult;
    const int ellipseResult = ellipseClonePreservesConfiguration();
    if (ellipseResult) return ellipseResult;
    const int spiralResult = spiralClonePreservesConfiguration();
    if (spiralResult) return spiralResult;
    const int starResult = starClonePreservesConfiguration();
    if (starResult) return starResult;
    const int factoryResult = factoriesPreserveXmlSupport();
    if (factoryResult) return factoryResult;
    return registrationIsIdempotentAndComplete();
}
