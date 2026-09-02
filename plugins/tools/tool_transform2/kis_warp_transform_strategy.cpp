/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_warp_transform_strategy.h"

#include <algorithm>

#include <PkPoint.h>
#include <PkPainter.h>
#include <PkPainterPath.h>

#include "kis_coordinates_converter.h"
#include "tool_transform_args.h"
#include "transform_transaction_properties.h"
#include "kis_painting_tweaks.h"
#include "kis_transform_utils.h"
#include "kis_algebra_2d.h"
#include "TransformToolPlatform.h"
#include "kis_signal_compressor.h"



struct KisWarpTransformStrategy::Private
{
    Private(KisWarpTransformStrategy *_q,
            const KisCoordinatesConverter *_converter,
            ToolTransformArgs &_currentArgs,
            TransformTransactionProperties &_transaction)
        : q(_q),
          converter(_converter),
          currentArgs(_currentArgs),
          transaction(_transaction),
          recalculateSignalCompressor(40, KisSignalCompressor::FIRST_ACTIVE)
    {
    }

    KisWarpTransformStrategy * const q;

    /// standard members ///

    const KisCoordinatesConverter *converter {0};

    //////
    ToolTransformArgs &currentArgs;
    //////
    TransformTransactionProperties &transaction;

    PkTransform paintingTransform;
    PkPointF paintingOffset;

    PkTransform handlesTransform;

    /// custom members ///

    PkImage transformedImage;

    int pointIndexUnderCursor {0};

    enum Mode {
        OVER_POINT = 0,
        MULTIPLE_POINT_SELECTION,
        MOVE_MODE,
        ROTATE_MODE,
        SCALE_MODE,
        NOTHING
    };
    Mode mode {NOTHING};

    PkVector<int> pointsInAction;
    int lastNumPoints {0};

    bool drawConnectionLines {false}; // useful while developing
    bool drawOrigPoints {false};
    bool drawTransfPoints {true};
    bool closeOnStartPointClick {false};
    bool clipOriginalPointsPosition {true};
    PkPointF pointPosOnClick;
    bool pointWasDragged {false};

    PkPointF lastMousePos;

    // cage transform also uses this logic. This helps this class know what transform type we are using
    TransformType transformType = TransformType::WARP_TRANSFORM;
    KisSignalCompressor recalculateSignalCompressor;
    PkConnection recalculateConnection;

    void recalculateTransformations();
    inline PkPointF imageToThumb(const PkPointF &pt, bool useFlakeOptimization);

    bool shouldCloseTheCage() const;
    PkVector<PkPointF*> getSelectedPoints(PkPointF *center, bool limitToSelectedOnly = false) const;
};

KisWarpTransformStrategy::KisWarpTransformStrategy(const KisCoordinatesConverter *converter,
                                                   KoSnapGuide *snapGuide,
                                                   ToolTransformArgs &currentArgs,
                                                   TransformTransactionProperties &transaction)
    : KisSimplifiedActionPolicyStrategy(converter, snapGuide),
      m_d(new Private(this, converter, currentArgs, transaction))
{
    m_d->recalculateConnection =
        PkObject::connect(&m_d->recalculateSignalCompressor,
                          &KisSignalCompressor::timeout,
                          &m_d->recalculateSignalCompressor,
                          [this]() { m_d->recalculateTransformations(); });
}

KisWarpTransformStrategy::~KisWarpTransformStrategy()
{
    PkObject::disconnect(m_d->recalculateConnection);
}

void KisWarpTransformStrategy::setTransformFunction(const PkPointF &mousePos, bool perspectiveModifierActive, bool shiftModifierActive, bool altModifierActive)
{
    (void)shiftModifierActive;
    (void)altModifierActive;

    const double handleRadius = KisTransformUtils::effectiveHandleGrabRadius(m_d->converter);

    bool cursorOverPoint = false;
    m_d->pointIndexUnderCursor = -1;

    KisTransformUtils::HandleChooser<Private::Mode>
        handleChooser(mousePos, Private::NOTHING);

    const PkVector<PkPointF> &points = m_d->currentArgs.transfPoints();
    for (int i = 0; i < points.size(); ++i) {
        if (handleChooser.addFunction(points[i],
                                      handleRadius, Private::NOTHING)) {

            cursorOverPoint = true;
            m_d->pointIndexUnderCursor = i;
        }
    }

    if (cursorOverPoint) {
        m_d->mode = perspectiveModifierActive &&
            !m_d->currentArgs.isEditingTransformPoints() ?
            Private::MULTIPLE_POINT_SELECTION : Private::OVER_POINT;

    } else if (!m_d->currentArgs.isEditingTransformPoints()) {
        PkPolygonF polygon(m_d->currentArgs.transfPoints());
        bool insidePolygon = polygon.boundingRect().contains(mousePos);
        m_d->mode = insidePolygon ? Private::MOVE_MODE :
            !perspectiveModifierActive ? Private::ROTATE_MODE :
            Private::SCALE_MODE;
    } else {
        m_d->mode = Private::NOTHING;
    }
}

TransformCursorDescriptor KisWarpTransformStrategy::getCurrentCursor() const
{
    TransformCursorDescriptor cursor;

    switch (m_d->mode) {
    case Private::OVER_POINT:
        cursor = TransformCursorDescriptor{TransformCursorKind::PointingHand};
        break;
    case Private::MULTIPLE_POINT_SELECTION:
        cursor = TransformCursorDescriptor{TransformCursorKind::Cross};
        break;
    case Private::MOVE_MODE:
        cursor = TransformCursorDescriptor{TransformCursorKind::SizeAll};
        break;
    case Private::ROTATE_MODE:
        cursor = TransformCursorDescriptor{TransformCursorKind::Cross};
        break;
    case Private::SCALE_MODE:
        cursor = TransformCursorDescriptor{TransformCursorKind::SizeVertical};
        break;
    case Private::NOTHING:
        cursor = TransformCursorDescriptor{TransformCursorKind::Arrow};
        break;
    }

    return cursor;
}

void KisWarpTransformStrategy::overrideDrawingItems(bool drawConnectionLines,
                                                    bool drawOrigPoints,
                                                    bool drawTransfPoints)
{
    m_d->drawConnectionLines = drawConnectionLines;
    m_d->drawOrigPoints = drawOrigPoints;
    m_d->drawTransfPoints = drawTransfPoints;
}

void KisWarpTransformStrategy::setCloseOnStartPointClick(bool value)
{
    m_d->closeOnStartPointClick = value;
}

void KisWarpTransformStrategy::setClipOriginalPointsPosition(bool value)
{
    m_d->clipOriginalPointsPosition = value;
}

void KisWarpTransformStrategy::setTransformType(TransformType type) {
    m_d->transformType = type;
}

void KisWarpTransformStrategy::drawConnectionLines(PkPainter &gc,
                                                   const PkVector<PkPointF> &origPoints,
                                                   const PkVector<PkPointF> &transfPoints,
                                                   bool isEditingPoints)
{
    (void)isEditingPoints;

    PkPen antsPen;
    PkPen outlinePen;

    KisPaintingTweaks::initAntsPen(&antsPen, &outlinePen);
    antsPen.setWidth(decorationThickness());
    outlinePen.setWidth(decorationThickness());

    const int numPoints = origPoints.size();

    for (int i = 0; i < numPoints; ++i) {
        gc.setPen(outlinePen);
        gc.drawLine(transfPoints[i], origPoints[i]);
        gc.setPen(antsPen);
        gc.drawLine(transfPoints[i], origPoints[i]);
    }
}

void KisWarpTransformStrategy::paint(TransformToolPainter &gc)
{
    // Draw preview image

    gc.save();

    gc.setOpacity(m_d->transaction.basePreviewOpacity());
    gc.setTransform(m_d->paintingTransform, true);
    gc.drawImage(m_d->paintingOffset, m_d->transformedImage);

    gc.restore();


    gc.save();
    gc.setTransform(m_d->handlesTransform, true);

    if (m_d->drawConnectionLines) {
        gc.setOpacity(0.5);

        drawConnectionLines(gc,
                            m_d->currentArgs.origPoints(),
                            m_d->currentArgs.transfPoints(),
                            m_d->currentArgs.isEditingTransformPoints());
    }


    PkPen mainPen(Qt::black);
    mainPen.setCosmetic(true);
    mainPen.setWidth(decorationThickness());
    PkPen outlinePen(Qt::white);
    outlinePen.setCosmetic(true);
    outlinePen.setWidth(decorationThickness());

    // draw handles
    {
        const int numPoints = m_d->currentArgs.origPoints().size();



        qreal handlesExtraScale = KisTransformUtils::scaleFromAffineMatrix(m_d->handlesTransform);

        qreal dstIn = 8 / handlesExtraScale;
        qreal dstOut = 10 / handlesExtraScale;
        qreal srcIn = 6 / handlesExtraScale;
        qreal srcOut = 6 / handlesExtraScale;

        PkRectF handleRect1(-0.5 * dstIn, -0.5 * dstIn, dstIn, dstIn);
        PkRectF handleRect2(-0.5 * dstOut, -0.5 * dstOut, dstOut, dstOut);

        if (m_d->drawTransfPoints) {
            gc.setOpacity(1.0);

            for (int i = 0; i < numPoints; ++i) {
                gc.setPen(outlinePen);
                gc.drawEllipse(handleRect2.translated(m_d->currentArgs.transfPoints()[i]));
                gc.setPen(mainPen);
                gc.drawEllipse(handleRect1.translated(m_d->currentArgs.transfPoints()[i]));
            }

            PkPointF center;
            PkVector<PkPointF*> selectedPoints = m_d->getSelectedPoints(&center, true);

            PkBrush selectionBrush(selectedPoints.size() > 1 ? Qt::red : Qt::black);

            PkBrush oldBrush = gc.brush();
            gc.setBrush(selectionBrush);
            for (const PkPointF *pt : selectedPoints) {
                gc.drawEllipse(handleRect1.translated(*pt));
            }
            gc.setBrush(oldBrush);

        }

        if (m_d->drawOrigPoints) {
            PkPainterPath inLine;
            inLine.moveTo(-0.5 * srcIn,            0);
            inLine.lineTo( 0.5 * srcIn,            0);
            inLine.moveTo(           0, -0.5 * srcIn);
            inLine.lineTo(           0,  0.5 * srcIn);

            PkPainterPath outLine;
            outLine.moveTo(-0.5 * srcOut, -0.5 * srcOut);
            outLine.lineTo( 0.5 * srcOut, -0.5 * srcOut);
            outLine.lineTo( 0.5 * srcOut,  0.5 * srcOut);
            outLine.lineTo(-0.5 * srcOut,  0.5 * srcOut);
            outLine.lineTo(-0.5 * srcOut, -0.5 * srcOut);

            gc.setOpacity(0.5);

            for (int i = 0; i < numPoints; ++i) {
                gc.setPen(outlinePen);
                gc.drawPath(outLine.translated(m_d->currentArgs.origPoints()[i]));
                gc.setPen(mainPen);
                gc.drawPath(inLine.translated(m_d->currentArgs.origPoints()[i]));
            }
        }

    }

    // draw grid lines only if we are using the GRID mode. Also only use this logic for warp, not cage transforms
    if (m_d->currentArgs.warpCalculation() == KisWarpTransformWorker::WarpCalculation::GRID &&
        m_d->transformType == TransformType::WARP_TRANSFORM ) {

    // see how many rows we have. we are only going to do lines up to 6 divisions/
    // it is almost impossible to use with 6 even.
    const int numPoints = m_d->currentArgs.origPoints().size();

    // grid is always square, so get the square root to find # of rows
    int rowsInWarp = sqrt(m_d->currentArgs.origPoints().size());


        TransformToolHandlePainter handlePainter(gc, 0.0, decorationThickness());
        handlePainter.setHandleStyle(TransformHandleStyle::PrimarySelection);

        // draw horizontal lines
        for (int i = 0; i < numPoints; i++) {
            if (i != 0 &&  i % rowsInWarp == rowsInWarp -1) {
                // skip line if it is the last in the row
            } else {
                handlePainter.drawConnectionLine(m_d->currentArgs.transfPoints()[i], m_d->currentArgs.transfPoints()[i+1]  );
            }
        }

        // draw vertical lines
        for (int i = 0; i < numPoints; i++) {

            if ( (numPoints - i - 1) < rowsInWarp ) {
                // last row doesn't need to draw vertical lines
            } else {
                handlePainter.drawConnectionLine(m_d->currentArgs.transfPoints()[i], m_d->currentArgs.transfPoints()[i+rowsInWarp] );
            }
        }

    } // end if statement

    gc.restore();
}

void KisWarpTransformStrategy::externalConfigChanged()
{
    if (m_d->lastNumPoints != m_d->currentArgs.transfPoints().size()) {
        m_d->pointsInAction.clear();
    }

    m_d->recalculateTransformations();
}

bool KisWarpTransformStrategy::beginPrimaryAction(const PkPointF &pt)
{
    const bool isEditingPoints = m_d->currentArgs.isEditingTransformPoints();
    bool retval = false;

    if (m_d->mode == Private::OVER_POINT ||
        m_d->mode == Private::MULTIPLE_POINT_SELECTION ||
        m_d->mode == Private::MOVE_MODE ||
        m_d->mode == Private::ROTATE_MODE ||
        m_d->mode == Private::SCALE_MODE) {

        retval = true;

    } else if (isEditingPoints) {
        PkPointF newPos = m_d->clipOriginalPointsPosition ?
            KisTransformUtils::clipInRect(pt, m_d->transaction.originalRect()) :
            pt;

        m_d->currentArgs.refOriginalPoints().append(newPos);
        m_d->currentArgs.refTransformedPoints().append(newPos);

        m_d->mode = Private::OVER_POINT;
        m_d->pointIndexUnderCursor = m_d->currentArgs.origPoints().size() - 1;

        m_d->recalculateSignalCompressor.start();

        retval = true;
    }

    if (m_d->mode == Private::OVER_POINT) {
        m_d->pointPosOnClick =
            m_d->currentArgs.transfPoints()[m_d->pointIndexUnderCursor];
        m_d->pointWasDragged = false;

        m_d->pointsInAction.clear();
        m_d->pointsInAction << m_d->pointIndexUnderCursor;
        m_d->lastNumPoints = m_d->currentArgs.transfPoints().size();
    } else if (m_d->mode == Private::MULTIPLE_POINT_SELECTION) {
        PkVector<int>::iterator it =
            std::find(m_d->pointsInAction.begin(),
                      m_d->pointsInAction.end(),
                      m_d->pointIndexUnderCursor);

        if (it != m_d->pointsInAction.end()) {
            m_d->pointsInAction.erase(it);
        } else {
            m_d->pointsInAction << m_d->pointIndexUnderCursor;
        }

        m_d->lastNumPoints = m_d->currentArgs.transfPoints().size();
    }

    m_d->lastMousePos = pt;
    return retval;
}

PkVector<PkPointF*> KisWarpTransformStrategy::Private::getSelectedPoints(PkPointF *center, bool limitToSelectedOnly) const
{
    PkVector<PkPointF> &points = currentArgs.refTransformedPoints();

    PkRectF boundingRect;
    PkVector<PkPointF*> selectedPoints;
    if (limitToSelectedOnly || pointsInAction.size() > 1) {
        for (int index : pointsInAction) {
            selectedPoints << &points[index];
            KisAlgebra2D::accumulateBounds(points[index], &boundingRect);
        }
    } else {
        PkVector<PkPointF>::iterator it = points.begin();
        PkVector<PkPointF>::iterator end = points.end();
        for (; it != end; ++it) {
            selectedPoints << &(*it);
            KisAlgebra2D::accumulateBounds(*it, &boundingRect);
        }
    }

    *center = boundingRect.center();
    return selectedPoints;
}

void KisWarpTransformStrategy::continuePrimaryAction(const PkPointF &pt, bool shiftModifierActive, bool altModifierActive)
{
    (void)shiftModifierActive;
    (void)altModifierActive;

    // toplevel code switches to HOVER mode if nothing is selected
    KIS_ASSERT_RECOVER_RETURN(m_d->mode == Private::MOVE_MODE ||
                              m_d->mode == Private::ROTATE_MODE ||
                              m_d->mode == Private::SCALE_MODE ||
                              (m_d->mode == Private::OVER_POINT &&
                               m_d->pointIndexUnderCursor >= 0 &&
                               m_d->pointsInAction.size() == 1) ||
                              (m_d->mode == Private::MULTIPLE_POINT_SELECTION &&
                               m_d->pointIndexUnderCursor >= 0));

    if (m_d->mode == Private::OVER_POINT) {
        if (m_d->currentArgs.isEditingTransformPoints()) {
            PkPointF newPos = m_d->clipOriginalPointsPosition ?
                KisTransformUtils::clipInRect(pt, m_d->transaction.originalRect()) :
                pt;
            m_d->currentArgs.origPoint(m_d->pointIndexUnderCursor) = newPos;
            m_d->currentArgs.transfPoint(m_d->pointIndexUnderCursor) = newPos;
        } else {
            m_d->currentArgs.transfPoint(m_d->pointIndexUnderCursor) = pt;
        }


        const qreal handleRadiusSq = pow2(KisTransformUtils::effectiveHandleGrabRadius(m_d->converter));
        qreal dist =
            kisSquareDistance(
                m_d->currentArgs.transfPoint(m_d->pointIndexUnderCursor),
                m_d->pointPosOnClick);

        if (dist > handleRadiusSq) {
            m_d->pointWasDragged = true;
        }
    } else if (m_d->mode == Private::MOVE_MODE) {
        PkPointF center;
        PkVector<PkPointF*> selectedPoints = m_d->getSelectedPoints(&center);

        PkPointF diff = pt - m_d->lastMousePos;

        PkVector<PkPointF*>::iterator it = selectedPoints.begin();
        PkVector<PkPointF*>::iterator end = selectedPoints.end();
        for (; it != end; ++it) {
            **it += diff;
        }
    } else if (m_d->mode == Private::ROTATE_MODE) {
        PkPointF center;
        PkVector<PkPointF*> selectedPoints = m_d->getSelectedPoints(&center);

        PkPointF oldDirection = m_d->lastMousePos - center;
        PkPointF newDirection = pt - center;

        qreal rotateAngle = KisAlgebra2D::angleBetweenVectors(oldDirection, newDirection);
        PkTransform R;
        R.rotateRadians(rotateAngle);

        PkTransform t =
            PkTransform::fromTranslate(-center.x(), -center.y()) *
            R *
            PkTransform::fromTranslate(center.x(), center.y());

        PkVector<PkPointF*>::iterator it = selectedPoints.begin();
        PkVector<PkPointF*>::iterator end = selectedPoints.end();
        for (; it != end; ++it) {
            **it = t.map(**it);
        }
    } else if (m_d->mode == Private::SCALE_MODE) {
        PkPointF center;
        PkVector<PkPointF*> selectedPoints = m_d->getSelectedPoints(&center);

        PkPolygonF polygon(m_d->currentArgs.origPoints());
        PkSizeF maxSize = polygon.boundingRect().size();
        qreal maxDimension = qMax(maxSize.width(), maxSize.height());

        qreal scale = 1.0 - (pt - m_d->lastMousePos).y() / maxDimension;

        PkTransform t =
            PkTransform::fromTranslate(-center.x(), -center.y()) *
            PkTransform::fromScale(scale, scale) *
            PkTransform::fromTranslate(center.x(), center.y());

        PkVector<PkPointF*>::iterator it = selectedPoints.begin();
        PkVector<PkPointF*>::iterator end = selectedPoints.end();
        for (; it != end; ++it) {
            **it = t.map(**it);
        }
    }

    m_d->lastMousePos = pt;
    m_d->recalculateSignalCompressor.start();

}

bool KisWarpTransformStrategy::Private::shouldCloseTheCage() const
{
    return currentArgs.isEditingTransformPoints() &&
        closeOnStartPointClick &&
        pointIndexUnderCursor == 0 &&
        currentArgs.origPoints().size() > 2 &&
        !pointWasDragged;
}

bool KisWarpTransformStrategy::acceptsClicks() const
{
    return m_d->shouldCloseTheCage() ||
        m_d->currentArgs.isEditingTransformPoints();
}

bool KisWarpTransformStrategy::endPrimaryAction()
{
    if (m_d->shouldCloseTheCage()) {
        m_d->currentArgs.setEditingTransformPoints(false);
    }

    return true;
}

inline PkPointF KisWarpTransformStrategy::Private::imageToThumb(const PkPointF &pt, bool useFlakeOptimization)
{
    return useFlakeOptimization ? converter->imageToDocument(converter->documentToFlake((pt))) : q->thumbToImageTransform().inverted().map(pt);
}

void KisWarpTransformStrategy::Private::recalculateTransformations()
{
    PkTransform scaleTransform = KisTransformUtils::imageToFlakeTransform(converter);

    PkTransform resultThumbTransform = q->thumbToImageTransform() * scaleTransform;
    qreal scale = KisTransformUtils::scaleFromAffineMatrix(resultThumbTransform);
    bool useFlakeOptimization = scale < 1.0 &&
        !KisTransformUtils::thumbnailTooSmall(resultThumbTransform, q->originalImage().rect());

    PkVector<PkPointF> thumbOrigPoints(currentArgs.numPoints());
    PkVector<PkPointF> thumbTransfPoints(currentArgs.numPoints());

    for (int i = 0; i < currentArgs.numPoints(); ++i) {
        thumbOrigPoints[i] = imageToThumb(currentArgs.origPoints()[i], useFlakeOptimization);
        thumbTransfPoints[i] = imageToThumb(currentArgs.transfPoints()[i], useFlakeOptimization);
    }

    paintingOffset = transaction.originalTopLeft();

    if (!q->originalImage().isNull() && !currentArgs.isEditingTransformPoints()) {
        PkPointF origTLInFlake = imageToThumb(transaction.originalTopLeft(), useFlakeOptimization);

        if (useFlakeOptimization) {
            transformedImage = q->originalImage().transformed(resultThumbTransform);
            paintingTransform = PkTransform();
        } else {
            transformedImage = q->originalImage();
            paintingTransform = resultThumbTransform;

        }

        transformedImage = q->calculateTransformedImage(currentArgs,
                                                        transformedImage,
                                                        thumbOrigPoints,
                                                        thumbTransfPoints,
                                                        origTLInFlake,
                                                        &paintingOffset);
    } else {
        transformedImage = q->originalImage();
        paintingOffset = imageToThumb(transaction.originalTopLeft(), false);
        paintingTransform = resultThumbTransform;
    }

    handlesTransform = scaleTransform;
    q->requestCanvasUpdate();
    q->requestImageRecalculation();
}

PkImage KisWarpTransformStrategy::calculateTransformedImage(ToolTransformArgs &currentArgs,
                                                           const PkImage &srcImage,
                                                           const PkVector<PkPointF> &origPoints,
                                                           const PkVector<PkPointF> &transfPoints,
                                                           const PkPointF &srcOffset,
                                                           PkPointF *dstOffset)
{
    return KisWarpTransformWorker::transformQImage(
        currentArgs.warpType(),
        origPoints, transfPoints,
        currentArgs.alpha(),
        srcImage,
        srcOffset, dstOffset);
}
