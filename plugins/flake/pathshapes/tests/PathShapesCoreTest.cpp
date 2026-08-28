/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "../PathShapesPlugin.h"
#include "../ellipse/EllipseShape.h"
#include "../ellipse/EllipseShapeConfigCommand.h"
#include "../ellipse/EllipseShapeFactory.h"
#include "../rectangle/RectangleShape.h"
#include "../rectangle/RectangleShapeConfigCommand.h"
#include "../rectangle/RectangleShapeFactory.h"
#include "../spiral/SpiralShape.h"
#include "../spiral/SpiralShapeConfigCommand.h"
#include "../spiral/SpiralShapeFactory.h"
#include "../star/StarShape.h"
#include "../star/StarShapeConfigCommand.h"
#include "../star/StarShapeFactory.h"

#include <KoShapeLoadingContext.h>
#include <KoShapeRegistry.h>
#include <KoXmlNS.h>
#include <PkXmlDocument.h>
#include <SvgLoadingContext.h>

#include <algorithm>
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

int representativePointsAndBoundsRemainLive()
{
    RectangleShape rectangle;
    rectangle.setSize(PkSizeF(200.0, 100.0));
    if (rectangle.subpaths().count() != 1 || rectangle.subpaths()[0]->count() != 4) return 30;
    qreal minX = 10000.0, minY = 10000.0, maxX = -10000.0, maxY = -10000.0;
    for (KoPathPoint *point : *rectangle.subpaths()[0]) {
        minX = std::min(minX, point->point().x());
        minY = std::min(minY, point->point().y());
        maxX = std::max(maxX, point->point().x());
        maxY = std::max(maxY, point->point().y());
    }
    if (!closeEnough(minX, 0.0) || !closeEnough(minY, 0.0) ||
        !closeEnough(maxX, 200.0) || !closeEnough(maxY, 100.0)) return 31;

    StarShape star;
    star.setConvex(true);
    star.setCornerCount(5);
    if (star.subpaths().count() != 1 || star.subpaths()[0]->count() != 5) return 32;

    EllipseShape ellipse;
    ellipse.setType(EllipseShape::Pie);
    ellipse.setStartAngle(0.0);
    ellipse.setEndAngle(90.0);
    if (ellipse.subpaths().count() != 1 || ellipse.subpaths()[0]->count() != 3) return 33;
    KoSubpath &pie = *ellipse.subpaths()[0];
    if (!pie[0]->hasProperty(KoPathPoint::HasControlPoint2) ||
        !pie[1]->hasProperty(KoPathPoint::HasControlPoint1) ||
        !pie[0]->hasProperty(KoPathPoint::CloseSubpath) ||
        !pie[2]->hasProperty(KoPathPoint::CloseSubpath)) return 34;
    if (!closeEnough(pie[0]->point().x(), 50.0) || !closeEnough(pie[0]->point().y(), 50.0) ||
        !closeEnough(pie[1]->point().x(), 0.0) || !closeEnough(pie[1]->point().y(), 0.0) ||
        !closeEnough(pie[2]->point().x(), 0.0) || !closeEnough(pie[2]->point().y(), 50.0)) return 35;
    if (!closeEnough(pie[0]->controlPoint2().x(), 50.0) ||
        !closeEnough(pie[0]->controlPoint2().y(), 22.3857625084603) ||
        !closeEnough(pie[1]->controlPoint1().x(), 27.6142374915397) ||
        !closeEnough(pie[1]->controlPoint1().y(), 0.0) ||
        !closeEnough(ellipse.size().width(), 50.0) ||
        !closeEnough(ellipse.size().height(), 50.0)) return 42;

    ellipse.setType(EllipseShape::Chord);
    if (ellipse.subpaths()[0]->count() != 2 ||
        !ellipse.subpaths()[0]->first()->hasProperty(KoPathPoint::CloseSubpath) ||
        !ellipse.subpaths()[0]->last()->hasProperty(KoPathPoint::CloseSubpath)) return 36;
    ellipse.setType(EllipseShape::Arc);
    if (ellipse.subpaths()[0]->first()->hasProperty(KoPathPoint::CloseSubpath) ||
        ellipse.subpaths()[0]->last()->hasProperty(KoPathPoint::CloseSubpath)) return 37;

    SpiralShape lineSpiral;
    lineSpiral.setType(SpiralShape::Line);
    if (lineSpiral.subpaths().count() != 1 || lineSpiral.subpaths()[0]->count() != 11) return 38;
    if (!lineSpiral.subpaths()[0]->first()->hasProperty(KoPathPoint::StartSubpath) ||
        !lineSpiral.subpaths()[0]->last()->hasProperty(KoPathPoint::StopSubpath)) return 45;
    for (KoPathPoint *point : *lineSpiral.subpaths()[0]) {
        if (point->hasProperty(KoPathPoint::HasControlPoint1) ||
            point->hasProperty(KoPathPoint::HasControlPoint2)) return 39;
    }
    if (!closeEnough(lineSpiral.size().width(), 85.5) ||
        !closeEnough(lineSpiral.size().height(), 95.0)) return 43;
    SpiralShape curveSpiral;
    if (curveSpiral.subpaths().count() != 1 || curveSpiral.subpaths()[0]->count() != 11) return 40;
    if (!curveSpiral.subpaths()[0]->first()->hasProperty(KoPathPoint::StartSubpath) ||
        !curveSpiral.subpaths()[0]->last()->hasProperty(KoPathPoint::StopSubpath)) return 46;
    if (!curveSpiral.subpaths()[0]->first()->hasProperty(KoPathPoint::HasControlPoint2) ||
        !curveSpiral.subpaths()[0]->last()->hasProperty(KoPathPoint::HasControlPoint1)) return 41;
    if (!closeEnough(curveSpiral.size().width(), 100.0) ||
        !closeEnough(curveSpiral.size().height(), 100.0)) return 44;
    return 0;
}

int realEllipseSaveSvgPreservesAttributes()
{
    EllipseShape circle;
    SvgSavingContext circleContext;
    if (!circle.saveSvg(circleContext)) return 80;
    const KoXmlWriter &circleWriter = circleContext.shapeWriter();
    if (circleWriter.elementName() != "circle" || circleWriter.attribute("r") != "50" ||
        circleWriter.attribute("cx") != "50" || circleWriter.attribute("cy") != "50") return 81;

    EllipseShape chord;
    chord.setType(EllipseShape::Chord);
    chord.setStartAngle(15.0);
    chord.setEndAngle(275.0);
    SvgSavingContext chordContext;
    if (!chord.saveSvg(chordContext)) return 82;
    const KoXmlWriter &chordWriter = chordContext.shapeWriter();
    if (chordWriter.elementName() != "path" ||
        chordWriter.attribute("sodipodi:type") != "arc" ||
        chordWriter.attribute("sodipodi:arc-type") != "chord" ||
        chordWriter.hasAttribute("sodipodi:open") || !chordWriter.hasAttribute("d")) return 83;

    EllipseShape arc;
    arc.setStartAngle(15.0);
    arc.setEndAngle(275.0);
    SvgSavingContext arcContext;
    if (!arc.saveSvg(arcContext) || arcContext.shapeWriter().attribute("sodipodi:open") != "true") return 84;
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

int xmlDefaultsAndErrorsRemainLive()
{
    KoShapeLoadingContext factoryContext(nullptr, nullptr);
    SvgLoadingContext svgContext;
    PkXmlDocument rectangleDocument;
    PkXmlElement rectangle = rectangleDocument.createElement("rect");
    rectangle.setAttribute("width", "100");
    rectangle.setAttribute("height", "50");
    rectangle.setAttribute("rx", "10");
    RectangleShape rectangleShape;
    if (!rectangleShape.loadSvg(rectangle, svgContext)) return 60;
    if (!closeEnough(rectangleShape.cornerRadiusX(), 20.0)) return 61;
    if (!closeEnough(rectangleShape.cornerRadiusY(), 40.0)) return 62;

    PkXmlDocument wrongDocument;
    PkXmlElement wrong = wrongDocument.createElement("rect");
    EllipseShape ellipse;
    if (ellipse.loadSvg(wrong, svgContext)) return 63;
    if (EllipseShapeFactory().supports(wrong, factoryContext)) return 64;
    return 0;
}

int configurationCommandsRedoAndUndo()
{
    RectangleShape rectangle;
    RectangleShapeConfigCommand rectangleCommand(&rectangle, 12.0, 34.0);
    rectangleCommand.redo();
    if (!closeEnough(rectangle.cornerRadiusX(), 12.0) || !closeEnough(rectangle.cornerRadiusY(), 34.0)) return 70;
    rectangleCommand.undo();
    if (!closeEnough(rectangle.cornerRadiusX(), 0.0) || !closeEnough(rectangle.cornerRadiusY(), 0.0)) return 71;

    EllipseShape ellipse;
    EllipseShapeConfigCommand ellipseCommand(&ellipse, EllipseShape::Pie, 15.0, 275.0);
    ellipseCommand.redo();
    if (ellipse.type() != EllipseShape::Pie || !closeEnough(ellipse.startAngle(), 15.0) ||
        !closeEnough(ellipse.endAngle(), 275.0)) return 72;
    ellipseCommand.undo();
    if (ellipse.type() != EllipseShape::Arc || !closeEnough(ellipse.startAngle(), 0.0) ||
        !closeEnough(ellipse.endAngle(), 0.0)) return 73;

    SpiralShape spiral;
    const SpiralShape::SpiralType oldType = spiral.type();
    const bool oldClockwise = spiral.clockWise();
    const qreal oldFade = spiral.fade();
    SpiralShapeConfigCommand spiralCommand(&spiral, SpiralShape::Line, !oldClockwise, 0.25);
    spiralCommand.redo();
    if (spiral.type() != SpiralShape::Line || spiral.clockWise() == oldClockwise || !closeEnough(spiral.fade(), 0.25)) return 74;
    spiralCommand.undo();
    if (spiral.type() != oldType || spiral.clockWise() != oldClockwise || !closeEnough(spiral.fade(), oldFade)) return 75;

    StarShape star;
    StarShapeConfigCommand starCommand(&star, 7, 10.0, 60.0, true);
    starCommand.redo();
    if (star.cornerCount() != 7 || !closeEnough(star.baseRadius(), 10.0) ||
        !closeEnough(star.tipRadius(), 60.0) || !star.convex()) return 76;
    starCommand.undo();
    if (star.cornerCount() != 5 || !closeEnough(star.baseRadius(), 25.0) ||
        !closeEnough(star.tipRadius(), 50.0) || star.convex()) return 77;
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
    const int geometryResult = representativePointsAndBoundsRemainLive();
    if (geometryResult) return geometryResult;
    const int factoryResult = factoriesPreserveXmlSupport();
    if (factoryResult) return factoryResult;
    const int xmlResult = xmlDefaultsAndErrorsRemainLive();
    if (xmlResult) return xmlResult;
    const int svgResult = realEllipseSaveSvgPreservesAttributes();
    if (svgResult) return svgResult;
    const int commandResult = configurationCommandsRedoAndUndo();
    if (commandResult) return commandResult;
    return registrationIsIdempotentAndComplete();
}
