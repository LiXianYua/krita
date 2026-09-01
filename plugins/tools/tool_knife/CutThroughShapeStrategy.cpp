/*
 *  SPDX-FileCopyrightText: 2025 Agata Cacko
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "CutThroughShapeStrategy.h"

#include <PkBrush.h>
#include <PkPainter.h>

#include <kis_algebra_2d.h>
#include <KoToolBase.h>
#include <KoCanvasBase.h>
#include <KoViewConverter.h>
#include <KoSelection.h>
#include <kis_global.h>
#include "kis_debug.h"
#include <KoPathShape.h>
#include <krita_utils.h>
#include <kis_coordinates_converter.h>
#include <kis_image.h>
#include <kis_shape_controller.h>
#include <PkPainterPath.h>
#include <KoShapeController.h>
#include <kundo2command.h>
#include <KoKeepShapesSelectedCommand.h>
#include <KoSvgTextShape.h>


CutThroughShapeStrategy::CutThroughShapeStrategy(KoToolBase *tool, KoSelection *selection, const PkList<KoShape *> &shapes, PkPointF startPoint, const GutterWidthsConfig &width)
    : KoInteractionStrategy(tool)
    , m_startPoint(startPoint)
    , m_endPoint(startPoint)
    , m_width(width)
{
    m_selectedShapes = selection->selectedEditableShapes();
    m_allShapes = shapes;
}

CutThroughShapeStrategy::~CutThroughShapeStrategy()
{

}

KUndo2Command *CutThroughShapeStrategy::createCommand()
{
    // TODO: undoing
    return 0;
}

PkPointF snapEndPoint(const PkPointF &startPoint, const PkPointF &mouseLocation, Qt::KeyboardModifiers modifiers) {

    PkPointF nicePoint = snapToClosestNiceAngle(mouseLocation, startPoint); // by default the function gives you 15 degrees increments

    if (modifiers & Qt::KeyboardModifier::ShiftModifier) {
        return nicePoint;
        if (qAbs(mouseLocation.x() - startPoint.x()) >= qAbs(mouseLocation.y() - startPoint.y())) {
            // do horizontal line
            return PkPointF(mouseLocation.x(), startPoint.y());
        } else {
            return PkPointF(startPoint.x(), mouseLocation.y());
        }
    }
    PkLineF line = PkLineF(startPoint, mouseLocation);
    qreal angle = line.angleTo(PkLineF(startPoint, nicePoint));
    qreal eps = kisDegreesToRadians(2.0f);
    if (angle < eps) {
        return nicePoint;
    }
    return mouseLocation;
}

void CutThroughShapeStrategy::handleMouseMove(const PkPointF &mouseLocation, Qt::KeyboardModifiers modifiers)
{
    m_endPoint = snapEndPoint(m_startPoint, mouseLocation, modifiers);
    PkRectF dirtyRect;
    KisAlgebra2D::accumulateBounds(m_startPoint, &dirtyRect);
    KisAlgebra2D::accumulateBounds(m_endPoint, &dirtyRect);
    dirtyRect = kisGrowRect(dirtyRect, gutterWidthInDocumentCoordinates(calculateLineAngle(m_startPoint, m_endPoint))); // twice as much as it should need to account for lines showing the effect

    PkRectF accumulatedWithPrevious = m_previousLineDirtyRect | dirtyRect;

    if (tool() && tool()->canvas()) {
        tool()->canvas()->updateCanvas(accumulatedWithPrevious);
    }
    m_previousLineDirtyRect = dirtyRect;

}


bool CutThroughShapeStrategy::willShapeBeCutGeneral(KoShape* referenceShape, const PkPainterPath& srcOutline, bool checkGapLineRect, const PkRectF& gapLineRect)
{
    if (dynamic_cast<KoSvgTextShape*>(referenceShape)) {
        // skip all text
        return false;
    }


    if (checkGapLineRect && (srcOutline.boundingRect() & gapLineRect).isEmpty()) {
        // the gap lines can't cross the shape since their bounding rects don't cross it
        return false;
    }

    return true;
}

bool CutThroughShapeStrategy::willShapeBeCutPrecise(const PkPainterPath& srcOutline, const PkLineF gapLine, const PkLineF& leftLine, const PkLineF& rightLine, const PkPolygonF& gapLinePolygon)
{
    bool containsGapLinePointStart = srcOutline.contains(gapLine.p1());
    bool containsGapLinePointEnd = srcOutline.contains(gapLine.p2());

    // if should skip if there is exactly one gap line point inside the shape
    bool exactlyOneGapLinePointInside = (containsGapLinePointStart != containsGapLinePointEnd);
    bool bothGapLinePointsInside = containsGapLinePointStart && containsGapLinePointEnd;

    if (exactlyOneGapLinePointInside) {
        return false;
    }

    bool crossesGapLine = KisAlgebra2D::getLineSegmentCrossingLineIndexes(leftLine, srcOutline).count() > 0
            || KisAlgebra2D::getLineSegmentCrossingLineIndexes(rightLine, srcOutline).count() > 0;


    // it doesn't contain exactly one point, therefore it contains either both or none.
    // if it contains both, it will be true.
    // if it contains none:
    //       if it crosses either gap line, it will be true
    //       if any of the shape points are inside the gap, it will be true
    //       otherwise it's false

    if (bothGapLinePointsInside) {
        return true;
    }

    if (crossesGapLine) {
        return true;
    }

    for (const PkPointF &p : srcOutline.toFillPolygon(PkTransform())) {
        if (gapLinePolygon.containsPoint(p, Qt::WindingFill)) {
            // a shape point is inside the gap shape
            return true;
        }
    }

    return false;

}

void CutThroughShapeStrategy::initializeOutlineObjects(const PkTransform &booleanWorkaroundTransform, PkList<KoShape *> allShapes, PkList<PkPainterPath> &outSrcOutlines, PkRectF &outOutlineRect)
{
    for (KoShape *shape : allShapes) {

        PkPainterPath outlineHere =
            booleanWorkaroundTransform.map(
            shape->absoluteTransformation().map(
                shape->outline()));

        outSrcOutlines << outlineHere;
        outOutlineRect |= outlineHere.boundingRect();
    }
}

void CutThroughShapeStrategy::initializeGapShapes(PkRectF outlineRect, PkLineF leftLine, PkLineF rightLine, PkPainterPath& outLeft, PkPainterPath& outRight,
                                                  PkRectF& outGapLineRect, PkPolygonF& outGapLinePolygon)
{


    PkRect outlineRectBiggerInt = kisGrowRect(outlineRect, 10).toRect();
    PkLineF leftLineLong = leftLine;
    PkLineF rightLineLong = rightLine;


    KisAlgebra2D::cropLineToRect(leftLineLong, outlineRectBiggerInt, true, true);
    KisAlgebra2D::cropLineToRect(rightLineLong, outlineRectBiggerInt, true, true);


    PkList<PkPainterPath> paths = KisAlgebra2D::getPathsFromRectangleCutThrough(PkRectF(outlineRectBiggerInt), leftLineLong, rightLineLong);
    outLeft = paths[0];
    outRight = paths[1];

    outGapLineRect = KisAlgebra2D::createRectFromCorners(leftLine) | KisAlgebra2D::createRectFromCorners(rightLine); // will not be empty if the gutterWidth > 0

    outGapLinePolygon = PkPolygonF({leftLine.p1(), leftLine.p2(), rightLine.p2(), rightLine.p1(), leftLine.p1()});

}

void CutThroughShapeStrategy::finishInteraction(Qt::KeyboardModifiers modifiers)
{
    tool()->canvas()->updateCanvas(m_previousLineDirtyRect);


    KisShapeController *shapeController =
        dynamic_cast<KisShapeController *>(tool()->canvas()->shapeController()->documentBase());
    KIS_SAFE_ASSERT_RECOVER_RETURN(shapeController);
    const PkTransform booleanWorkaroundTransform =
        KritaUtils::pathShapeBooleanSpaceWorkaround(shapeController->currentImage());

    PkList<PkPainterPath> srcOutlines;
    PkRectF outlineRect;

    if (m_allShapes.length() == 0) {
        return;
    }

    initializeOutlineObjects(booleanWorkaroundTransform, m_allShapes, srcOutlines, outlineRect);



    if (outlineRect.isEmpty()) {
        //qCritical() << "The outline rect is empty";
        return;
    }


    PkLineF gapLine = PkLineF(m_startPoint, m_endPoint);
    qreal eps = 0.0000001;
    if (gapLine.length() < eps) {
        return;
    }

    qreal gutterWidth = gutterWidthInDocumentCoordinates(calculateLineAngle(m_startPoint, m_endPoint));

    PkList<PkLineF> gapLines = KisAlgebra2D::getParallelLines(gapLine, gutterWidth/2);

    gapLine = booleanWorkaroundTransform.map(gapLine);
    gapLines[0] = booleanWorkaroundTransform.map(gapLines[0]);
    gapLines[1] = booleanWorkaroundTransform.map(gapLines[1]);

    PkLineF leftLine = gapLines[0];
    PkLineF rightLine = gapLines[1];


    if (leftLine.length() == 0 || rightLine.length() == 0) {
        KIS_SAFE_ASSERT_RECOVER_RETURN(gapLine.length() != 0 && gapLines[0].length() != 0 && gapLines[1].length() != 0 && "Original gap lines shouldn't be empty at this point");
        return;
    }

    // -------------

    PkPainterPath left, right;

    PkRectF gapLineRect;
    PkPolygonF gapLinePolygon;
    initializeGapShapes(outlineRect, leftLine, rightLine, left, right, gapLineRect, gapLinePolygon);


    bool checkGapLineRect = !gapLineRect.isEmpty();

    PkList<KoShape*> newSelectedShapes;
    PkList<KoShape*> shapesToRemove;
    int affectedShapes = 0;
    PkTransform booleanWorkaroundTransformInverted = booleanWorkaroundTransform.inverted();


    std::unique_ptr<KUndo2Command> cmd = std::unique_ptr<KUndo2Command>(new KUndo2Command(kundo2_i18n("Knife tool: cut through shapes")));
    new KoKeepShapesSelectedCommand(m_selectedShapes, {}, tool()->canvas()->selectedShapesProxy(), false, cmd.get());


    for (int i = 0; i < srcOutlines.size(); i++) {

        KoShape* referenceShape = m_allShapes[i];
        bool wasSelected = m_selectedShapes.contains(referenceShape);

        bool skipThisShape = !willShapeBeCutGeneral(referenceShape, srcOutlines[i], checkGapLineRect, gapLineRect);
        skipThisShape = skipThisShape || !willShapeBeCutPrecise(srcOutlines[i], gapLine, leftLine, rightLine, gapLinePolygon);

        if (skipThisShape) {
            if (wasSelected) {
                newSelectedShapes << referenceShape;
            }
            continue;
        }

        affectedShapes++;


        PkPainterPath leftPath = srcOutlines[i] & left;
        PkPainterPath rightPath = srcOutlines[i] & right;

        PkList<PkPainterPath> bothSides;
        bothSides << leftPath << rightPath;


        for (PkPainterPath path : bothSides) {
            if (path.isEmpty()) {
                continue;
            }

            // comment copied from another place:
            // there is a bug in Qt, sometimes it leaves the resulting
            // outline open, so just close it explicitly.
            path.closeSubpath();
            // this is needed because Qt linearize curves; this allows for a
            // "sane" linearization instead of a very blocky appearance
            path = booleanWorkaroundTransformInverted.map(path);
            std::unique_ptr<KoPathShape> shape = std::unique_ptr<KoPathShape>(KoPathShape::createShapeFromPainterPath(path));
            shape->closeMerge();

            if (shape->boundingRect().isEmpty()) {
                continue;
            }

            shape->setBackground(referenceShape->background());
            shape->setStroke(referenceShape->stroke());
            shape->setZIndex(referenceShape->zIndex());

            KoShapeContainer *parent = referenceShape->parent();

            if (wasSelected) {
                newSelectedShapes << shape.get();
            }

            tool()->canvas()->shapeController()->addShapeDirect(shape.release(), parent, cmd.get());

        }

        // that happens no matter if there was any non-empty shape
        // because if there is none, maybe they just were underneath the gap
        shapesToRemove << m_allShapes[i];

    }

    if (affectedShapes > 0) {
        tool()->canvas()->shapeController()->removeShapes(shapesToRemove, cmd.get());
        new KoKeepShapesSelectedCommand({}, newSelectedShapes, tool()->canvas()->selectedShapesProxy(), true, cmd.get());
        tool()->canvas()->addCommand(cmd.release());
    }



}

void CutThroughShapeStrategy::paint(PkPainter &painter, const KoViewConverter &converter)
{
    painter.save();

    PkColor semitransparentGray = PkColor(Qt::darkGray);
    semitransparentGray.setAlphaF(0.6);
    PkPen pen(PkBrush(semitransparentGray), 2);
    painter.setPen(pen);

    painter.setRenderHint(PkPainter::RenderHint::Antialiasing, true);

    qreal gutterWidth = gutterWidthInDocumentCoordinates(calculateLineAngle(m_startPoint, m_endPoint));

    PkLineF gutterCenterLine = PkLineF(m_startPoint, m_endPoint);
    gutterCenterLine = converter.documentToView().map(gutterCenterLine);
    PkLineF gutterWidthHelperLine = PkLineF(PkPointF(0, 0), PkPointF(gutterWidth, 0));
    gutterWidthHelperLine = converter.documentToView().map(gutterWidthHelperLine);

    gutterWidth = gutterWidthHelperLine.length();

    PkList<PkLineF> gutterLines = KisAlgebra2D::getParallelLines(gutterCenterLine, gutterWidth/2);

    PkLineF gutterLine1 = gutterLines.length() > 0 ? gutterLines[0] : gutterCenterLine;
    PkLineF gutterLine2 = gutterLines.length() > 1 ? gutterLines[1] : gutterCenterLine;


    painter.drawLine(gutterLine1);
    painter.drawLine(gutterLine2);

    PkRectF arcRect1 = PkRectF(gutterCenterLine.p1() - PkPointF(gutterWidth/2, gutterWidth/2), gutterCenterLine.p1() + PkPointF(gutterWidth/2, gutterWidth/2));
    PkRectF arcRect2 = PkRectF(gutterCenterLine.p2() - PkPointF(gutterWidth/2, gutterWidth/2), gutterCenterLine.p2() + PkPointF(gutterWidth/2, gutterWidth/2));

    int qtAngleFactor = 16;
    int qtHalfCircle = qtAngleFactor*180;

    painter.drawArc(arcRect1, -qtAngleFactor*kisRadiansToDegrees(KisAlgebra2D::directionBetweenPoints(gutterCenterLine.p1(), gutterLine1.p1(), 0)), qtHalfCircle);
    painter.drawArc(arcRect2, -qtAngleFactor*kisRadiansToDegrees(KisAlgebra2D::directionBetweenPoints(gutterCenterLine.p2(), gutterLine1.p2(), 0)), -qtHalfCircle);


    int xLength = 3;
    qreal xLengthEllipse = 2 * std::sqrt(2.0);

    if (false) { // drawing X
    painter.drawLine({PkLineF(gutterCenterLine.p1() - PkPointF(xLength, xLength), gutterCenterLine.p1() + PkPointF(xLength, xLength))});
    painter.drawLine({PkLineF(gutterCenterLine.p2() - PkPointF(xLength, xLength), gutterCenterLine.p2() + PkPointF(xLength, xLength))});

    painter.drawLine({PkLineF(gutterCenterLine.p1() - PkPointF(xLength, -xLength), gutterCenterLine.p1() + PkPointF(xLength, -xLength))});
    painter.drawLine({PkLineF(gutterCenterLine.p2() - PkPointF(xLength, -xLength), gutterCenterLine.p2() + PkPointF(xLength, -xLength))});
    }

    // ellipse at the both ends of the gutter center line
    painter.drawEllipse(gutterCenterLine.p1(), xLengthEllipse, xLengthEllipse);
    painter.drawEllipse(gutterCenterLine.p2(), xLengthEllipse, xLengthEllipse);



    pen.setWidth(1);
    semitransparentGray.setAlphaF(0.2);
    pen.setColor(semitransparentGray);

    painter.setPen(pen);

    painter.drawLine(gutterCenterLine);

    painter.restore();
}

qreal CutThroughShapeStrategy::gutterWidthInDocumentCoordinates(qreal lineAngle)
{
    const KisCoordinatesConverter *converter =
        dynamic_cast<const KisCoordinatesConverter *>(tool()->canvas()->viewConverter());
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(converter, m_width.widthForAngleInPixels(lineAngle));
    PkLineF helperGapWidthLine = PkLineF(PkPointF(0, 0), PkPointF(0, m_width.widthForAngleInPixels(lineAngle)));
    PkLineF helperGapWidthLineTransformed = converter->imageToDocument(helperGapWidthLine);
    return helperGapWidthLineTransformed.length();
}

qreal CutThroughShapeStrategy::calculateLineAngle(PkPointF start, PkPointF end)
{
    PkPointF vec = end - start;
    qreal angleDegrees = KisAlgebra2D::wrapValue(kisRadiansToDegrees(std::atan2(vec.y(), vec.x())), 0.0, 360.0);
    return angleDegrees;
}
