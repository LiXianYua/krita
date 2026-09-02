/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *  SPDX-FileCopyrightText: 2022 Carsten Hartenfels <carsten.hartenfels@pm.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_perspective_transform_strategy.h"

#include <PkPoint.h>
#include <PkPainter.h>
#include <PkPainterPath.h>
#include <PkMatrix4x4.h>
#include <PkVectorND.h>

#include <Eigen/Dense>

#include "kis_coordinates_converter.h"
#include "tool_transform_args.h"
#include "transform_transaction_properties.h"
#include "krita_utils.h"
#include "kis_algebra_2d.h"
#include "kis_transform_utils.h"
#include "kis_free_transform_strategy_gsl_helpers.h"

namespace {
enum StrokeFunction {
    DRAG_HANDLE = 0,
    DRAG_X_VANISHING_POINT,
    DRAG_Y_VANISHING_POINT,
    MOVE,
    NONE
};

enum HandleIndexes {
    HANDLE_TOP_LEFT = 0,
    HANDLE_TOP_RIGHT,
    HANDLE_BOTTOM_LEFT,
    HANDLE_BOTTOM_RIGHT,
    HANDLE_MIDDLE_TOP,
    HANDLE_MIDDLE_BOTTOM,
    HANDLE_MIDDLE_LEFT,
    HANDLE_MIDDLE_RIGHT,
    HANDLE_COUNT,
};
}

struct KisPerspectiveTransformStrategy::Private
{
    Private(KisPerspectiveTransformStrategy *_q,
            const KisCoordinatesConverter *_converter,
            ToolTransformArgs &_currentArgs,
            TransformTransactionProperties &_transaction)
        : q(_q),
          converter(_converter),
          currentArgs(_currentArgs),
          transaction(_transaction),
          imageTooBig(false),
          isTransforming(false)
    {
    }

    KisPerspectiveTransformStrategy *q;

    /// standard members ///

    const KisCoordinatesConverter *converter;

    //////
    ToolTransformArgs &currentArgs;
    //////
    TransformTransactionProperties &transaction;


    PkTransform thumbToImageTransform;
    PkImage originalImage;

    PkTransform paintingTransform;
    PkPointF paintingOffset;

    PkTransform handlesTransform;

    /// custom members ///

    StrokeFunction function {NONE};

    struct HandlePoints {
        bool xVanishingExists {false};
        bool yVanishingExists {false};

        PkPointF xVanishing;
        PkPointF yVanishing;
    };
    HandlePoints transformedHandles;

    PkTransform transform;

    PkVector<PkPointF> srcHandlePoints;
    PkVector<PkPointF> dstHandlePoints;
    int currentDraggingHandlePoint {0};

    bool imageTooBig {false};

    PkPointF clickPos;
    ToolTransformArgs clickArgs;
    bool isTransforming {false};

    TransformCursorDescriptor getScaleCursor(const PkPointF &handlePt);
    TransformCursorDescriptor getShearCursor(const PkPointF &start, const PkPointF &end);
    void recalculateTransformations();
    void recalculateTransformedHandles();

    void transformIntoArgs(const Eigen::Matrix3f &t);
    PkTransform transformFromArgs();
};

KisPerspectiveTransformStrategy::KisPerspectiveTransformStrategy(const KisCoordinatesConverter *converter,
                                                                 KoSnapGuide *snapGuide,
                                                   ToolTransformArgs &currentArgs,
                                                   TransformTransactionProperties &transaction)
    : KisSimplifiedActionPolicyStrategy(converter, snapGuide),
      m_d(new Private(this, converter, currentArgs, transaction))
{
}

KisPerspectiveTransformStrategy::~KisPerspectiveTransformStrategy()
{
}

void KisPerspectiveTransformStrategy::Private::recalculateTransformedHandles()
{
    srcHandlePoints.resize(HANDLE_COUNT);
    srcHandlePoints[HANDLE_TOP_LEFT] = transaction.originalTopLeft();
    srcHandlePoints[HANDLE_TOP_RIGHT] = transaction.originalTopRight();
    srcHandlePoints[HANDLE_BOTTOM_LEFT] = transaction.originalBottomLeft();
    srcHandlePoints[HANDLE_BOTTOM_RIGHT] = transaction.originalBottomRight();
    srcHandlePoints[HANDLE_MIDDLE_TOP] = transaction.originalMiddleTop();
    srcHandlePoints[HANDLE_MIDDLE_BOTTOM] = transaction.originalMiddleBottom();
    srcHandlePoints[HANDLE_MIDDLE_LEFT] = transaction.originalMiddleLeft();
    srcHandlePoints[HANDLE_MIDDLE_RIGHT] = transaction.originalMiddleRight();

    dstHandlePoints.clear();
    for (const PkPointF &pt : srcHandlePoints) {
        dstHandlePoints << transform.map(pt);
    }

    PkMatrix4x4 realMatrix(transform);
    PkVector4D v;

    v = PkVector4D(1, 0, 0, 0);
    v = realMatrix * v;
    transformedHandles.xVanishingExists = !qFuzzyCompare(v.w(), 0);
    transformedHandles.xVanishing = v.toVector2DAffine().toPointF();

    v = PkVector4D(0, 1, 0, 0);
    v = realMatrix * v;
    transformedHandles.yVanishingExists = !qFuzzyCompare(v.w(), 0);
    transformedHandles.yVanishing = v.toVector2DAffine().toPointF();
}

void KisPerspectiveTransformStrategy::setTransformFunction(const PkPointF &mousePos, bool perspectiveModifierActive, bool shiftModifierActive, bool altModifierActive)
{
    (void)perspectiveModifierActive;
    (void)shiftModifierActive;
    (void)altModifierActive;

    PkPolygonF transformedPolygon = m_d->transform.map(PkPolygonF(m_d->transaction.originalRect()));
    StrokeFunction defaultFunction = transformedPolygon.containsPoint(mousePos, Qt::OddEvenFill) ? MOVE : NONE;
    KisTransformUtils::HandleChooser<StrokeFunction>
        handleChooser(mousePos, defaultFunction);

    qreal handleRadius = KisTransformUtils::effectiveHandleGrabRadius(m_d->converter);

    if (!m_d->transformedHandles.xVanishing.isNull()) {
        handleChooser.addFunction(m_d->transformedHandles.xVanishing,
                                  handleRadius, DRAG_X_VANISHING_POINT);
    }

    if (!m_d->transformedHandles.yVanishing.isNull()) {
        handleChooser.addFunction(m_d->transformedHandles.yVanishing,
                                  handleRadius, DRAG_Y_VANISHING_POINT);
    }

    m_d->currentDraggingHandlePoint = -1;
    for (int i = 0; i < m_d->dstHandlePoints.size(); i++) {
        if (handleChooser.addFunction(m_d->dstHandlePoints[i],
                                      handleRadius, DRAG_HANDLE)) {

            m_d->currentDraggingHandlePoint = i;
        }
    }

    m_d->function = handleChooser.function();
}

TransformCursorDescriptor KisPerspectiveTransformStrategy::getCurrentCursor() const
{
    TransformCursorDescriptor cursor;

    switch (m_d->function) {
    case NONE:
        cursor = TransformCursorDescriptor{TransformCursorKind::Arrow};
        break;
    case MOVE:
        cursor = TransformCursorDescriptor{TransformCursorKind::SizeAll};
        break;
    case DRAG_HANDLE:
    case DRAG_X_VANISHING_POINT:
    case DRAG_Y_VANISHING_POINT:
        cursor = TransformCursorDescriptor{TransformCursorKind::PointingHand};
        break;
    }

    return cursor;
}

void KisPerspectiveTransformStrategy::paint(TransformToolPainter &gc)
{
    gc.save();

    gc.setOpacity(m_d->transaction.basePreviewOpacity());
    gc.setTransform(m_d->paintingTransform, true);
    gc.drawImage(m_d->paintingOffset, originalImage());

    gc.restore();

    // Draw Handles
    PkPainterPath handles;

    handles.moveTo(m_d->transaction.originalTopLeft());
    handles.lineTo(m_d->transaction.originalTopRight());
    handles.lineTo(m_d->transaction.originalBottomRight());
    handles.lineTo(m_d->transaction.originalBottomLeft());
    handles.lineTo(m_d->transaction.originalTopLeft());


    auto addHandleRectFunc =
        [&](const PkPointF &pt) {
            handles.addRect(
                KisTransformUtils::handleRect(KisTransformUtils::handleVisualRadius,
                                              m_d->handlesTransform,
                                              m_d->transaction.originalRect(), pt)
                .translated(pt));
    };

    addHandleRectFunc(m_d->transaction.originalTopLeft());
    addHandleRectFunc(m_d->transaction.originalTopRight());
    addHandleRectFunc(m_d->transaction.originalBottomLeft());
    addHandleRectFunc(m_d->transaction.originalBottomRight());
    addHandleRectFunc(m_d->transaction.originalMiddleTop());
    addHandleRectFunc(m_d->transaction.originalMiddleBottom());
    addHandleRectFunc(m_d->transaction.originalMiddleLeft());
    addHandleRectFunc(m_d->transaction.originalMiddleRight());

    gc.save();

    if (m_d->isTransforming) {
        gc.setOpacity(0.1);
    }

    /**
     * WARNING: we cannot install a transform to paint the handles here!
     *
     * There is a bug in Qt that prevents painting of cosmetic-pen
     * brushes in openGL mode when a TxProject matrix is active on
     * a PkPainter. So just convert it manually.
     *
     * https://bugreports.qt-project.org/browse/QTBUG-42658
     */

    //gc.setTransform(m_d->handlesTransform, true); <-- don't do like this!

    PkPainterPath mappedHandles = m_d->handlesTransform.map(handles);

    PkPen pen[2];
    pen[0].setWidth(decorationThickness());
    pen[0].setCosmetic(true);
    pen[1].setWidth(decorationThickness() * 2);
    pen[1].setCosmetic(true);
    pen[1].setColor(Qt::lightGray);

    for (int i = 1; i >= 0; --i) {
        gc.setPen(pen[i]);
        gc.drawPath(mappedHandles);
    }

    gc.restore();

    { // painting perspective handles
        PkPainterPath perspectiveHandles;

        PkRectF handleRect =
            KisTransformUtils::handleRect(KisTransformUtils::handleVisualRadius,
                                          PkTransform(),
                                          m_d->transaction.originalRect(), 0, 0);

        if (m_d->transformedHandles.xVanishingExists) {
            PkRectF rc = handleRect.translated(m_d->transformedHandles.xVanishing);
            perspectiveHandles.addEllipse(rc);
        }

        if (m_d->transformedHandles.yVanishingExists) {
            PkRectF rc = handleRect.translated(m_d->transformedHandles.yVanishing);
            perspectiveHandles.addEllipse(rc);
        }

        if (!perspectiveHandles.isEmpty()) {
            gc.save();
            gc.setTransform(m_d->converter->imageToWidgetTransform());

            gc.setBrush(PkBrush(Qt::red));

            for (int i = 1; i >= 0; --i) {
                gc.setPen(pen[i]);
                gc.drawPath(perspectiveHandles);
            }

            gc.restore();
        }
    }
}

void KisPerspectiveTransformStrategy::externalConfigChanged()
{
    m_d->recalculateTransformations();
}

bool KisPerspectiveTransformStrategy::beginPrimaryAction(const PkPointF &pt)
{
    if (m_d->function == NONE) return false;

    m_d->clickPos = pt;
    m_d->clickArgs = m_d->currentArgs;

    return true;
}

Eigen::Matrix3f getTransitionMatrix(const PkVector<PkPointF> &sp)
{
    Eigen::Matrix3f A;
    Eigen::Vector3f v3;

    A << sp[HANDLE_TOP_LEFT].x() , sp[HANDLE_TOP_RIGHT].x() , sp[HANDLE_BOTTOM_LEFT].x()
        ,sp[HANDLE_TOP_LEFT].y() , sp[HANDLE_TOP_RIGHT].y() , sp[HANDLE_BOTTOM_LEFT].y()
        ,                    1 ,                        1   ,                        1;

    v3 << sp[HANDLE_BOTTOM_RIGHT].x() , sp[HANDLE_BOTTOM_RIGHT].y() , 1;

    Eigen::Vector3f coeffs = A.colPivHouseholderQr().solve(v3);

    A.col(0) *= coeffs(0);
    A.col(1) *= coeffs(1);
    A.col(2) *= coeffs(2);

    return A;
}

PkTransform toQTransform(const Eigen::Matrix3f &m)
{
    return PkTransform(m(0,0), m(1,0), m(2,0),
                      m(0,1), m(1,1), m(2,1),
                      m(0,2), m(1,2), m(2,2));
}

Eigen::Matrix3f fromQTransform(const PkTransform &t)
{
    Eigen::Matrix3f m;

    m << t.m11() , t.m21() , t.m31()
        ,t.m12() , t.m22() , t.m32()
        ,t.m13() , t.m23() , t.m33();

    return m;
}

Eigen::Matrix3f fromTranslate(const PkPointF &pt)
{
    Eigen::Matrix3f m;

    m << 1 , 0 , pt.x()
        ,0 , 1 , pt.y()
        ,0 , 0 , 1;

    return m;
}

Eigen::Matrix3f fromScale(qreal sx, qreal sy)
{
    Eigen::Matrix3f m;

    m << sx , 0 , 0
        ,0 , sy , 0
        ,0 , 0 , 1;

    return m;
}

Eigen::Matrix3f fromShear(qreal sx, qreal sy)
{
    Eigen::Matrix3f m;

    m << 1 , sx , 0
        ,sy , sx*sy + 1, 0
        ,0 , 0 , 1;

    return m;
}

void KisPerspectiveTransformStrategy::Private::transformIntoArgs(const Eigen::Matrix3f &t)
{
    Eigen::Matrix3f TS = fromTranslate(-currentArgs.originalCenter());

    Eigen::Matrix3f m = t * TS.inverse();

    qreal tX = m(0,2) / m(2,2);
    qreal tY = m(1,2) / m(2,2);

    Eigen::Matrix3f T = fromTranslate(PkPointF(tX, tY));

    m = T.inverse() * m;

    /**
     * We disabled decomposed transformation due to bug
     * https://bugs.kde.org/show_bug.cgi?id=447255
     *
     * In some cases decomposed preliminary transformation
     * shrinks the image into a very small size, which is later
     * inflated by the perspective transform. It creates a really
     * bad and blurry result.
     *
     * Even though the usage of preliminary rotation makes the bug
     * much less obvious, but the image is still really blurred.
     */

#if 0
    // Decomposition according to:
    // https://www.w3.org/TR/css-transforms-1/#decomposing-a-3d-matrix
    KisAlgebra2D::DecomposedMatrix dm(toQTransform(m));

    currentArgs.setScaleX(dm.scaleX);
    currentArgs.setScaleY(dm.scaleY);

    currentArgs.setShearX(dm.shearXY);
    currentArgs.setShearY(0.0);

    currentArgs.setAZ(kisDegreesToRadians(dm.angle));

    PkTransform pre = dm.scaleTransform() * dm.shearTransform() * dm.rotateTransform();
    m = m * fromQTransform(pre.inverted());
#else
    currentArgs.setScaleX(1.0);
    currentArgs.setScaleY(1.0);
    currentArgs.setShearX(0.0);
    currentArgs.setShearY(0.0);
    currentArgs.setAZ(0.0);
#endif

    currentArgs.setTransformedCenter(PkPointF(tX, tY));
    currentArgs.setFlattenedPerspectiveTransform(toQTransform(m));
}

PkTransform KisPerspectiveTransformStrategy::Private::transformFromArgs()
{
    KisTransformUtils::MatricesPack m(currentArgs);
    return m.finalTransform();
}

PkVector4D fromQPointF(const PkPointF &pt) {
    return PkVector4D(pt.x(), pt.y(), 0, 1.0);
}

PkPointF toQPointF(const PkVector4D &v) {
    return v.toVector2DAffine().toPointF();
}

void KisPerspectiveTransformStrategy::continuePrimaryAction(const PkPointF &mousePos, bool shiftModifierActive, bool altModifierActive)
{
    (void)shiftModifierActive;
    (void)altModifierActive;

    m_d->isTransforming = true;

    switch (m_d->function) {
    case NONE:
        break;
    case MOVE: {
        PkPointF diff = mousePos - m_d->clickPos;
        m_d->currentArgs.setTransformedCenter(
            m_d->clickArgs.transformedCenter() + diff);
        break;
    }
    case DRAG_HANDLE: {
        KIS_ASSERT_RECOVER_RETURN(m_d->currentDraggingHandlePoint >= 0);
        KIS_ASSERT_RECOVER_RETURN(m_d->currentDraggingHandlePoint < HANDLE_COUNT);
        if (m_d->currentDraggingHandlePoint < HANDLE_MIDDLE_TOP) {
            // Corner point, transform directly.
            m_d->dstHandlePoints[m_d->currentDraggingHandlePoint] = mousePos;
        } else {
            // Middle point, move adjacent corners.
            PkPointF delta = mousePos - m_d->dstHandlePoints[m_d->currentDraggingHandlePoint];
            switch(m_d->currentDraggingHandlePoint) {
            case HANDLE_MIDDLE_TOP:
                m_d->dstHandlePoints[HANDLE_TOP_LEFT] += delta;
                m_d->dstHandlePoints[HANDLE_TOP_RIGHT] += delta;
                break;
            case HANDLE_MIDDLE_BOTTOM:
                m_d->dstHandlePoints[HANDLE_BOTTOM_LEFT] += delta;
                m_d->dstHandlePoints[HANDLE_BOTTOM_RIGHT] += delta;
                break;
            case HANDLE_MIDDLE_LEFT:
                m_d->dstHandlePoints[HANDLE_TOP_LEFT] += delta;
                m_d->dstHandlePoints[HANDLE_BOTTOM_LEFT] += delta;
                break;
            case HANDLE_MIDDLE_RIGHT:
                m_d->dstHandlePoints[HANDLE_TOP_RIGHT] += delta;
                m_d->dstHandlePoints[HANDLE_BOTTOM_RIGHT] += delta;
                break;
            }
        }

        Eigen::Matrix3f A = getTransitionMatrix(m_d->srcHandlePoints);
        Eigen::Matrix3f B = getTransitionMatrix(m_d->dstHandlePoints);
        Eigen::Matrix3f result = B * A.inverse();

        // Points in dstHandlePoints are not arranged in an organized way, so if we used it directly as a polygon it
        // will never be considered convex
        PkPolygonF poly;
        poly << m_d->dstHandlePoints[HANDLE_TOP_LEFT] << m_d->dstHandlePoints[HANDLE_TOP_RIGHT]
             << m_d->dstHandlePoints[HANDLE_BOTTOM_RIGHT] << m_d->dstHandlePoints[HANDLE_BOTTOM_LEFT];

        //Don't apply the handle movement if it makes the transform area not convex
        if (!KisAlgebra2D::isPolygonTrulyConvex(poly)) {
            break;
        }

        m_d->transformIntoArgs(result);

        break;
    }
    case DRAG_X_VANISHING_POINT:
    case DRAG_Y_VANISHING_POINT: {

        PkMatrix4x4 m(m_d->transform);

        PkPointF tl = m_d->transaction.originalTopLeft();
        PkPointF tr = m_d->transaction.originalTopRight();
        PkPointF bl = m_d->transaction.originalBottomLeft();
        PkPointF br = m_d->transaction.originalBottomRight();

        PkVector4D v(1,0,0,0);
        PkVector4D otherV(0,1,0,0);

        if (m_d->function == DRAG_X_VANISHING_POINT) {
            v = PkVector4D(1,0,0,0);
            otherV = PkVector4D(0,1,0,0);
        } else {
            v = PkVector4D(0,1,0,0);
            otherV = PkVector4D(1,0,0,0);
        }

        PkPointF tl_dst = toQPointF(m * fromQPointF(tl));
        PkPointF tr_dst = toQPointF(m * fromQPointF(tr));
        PkPointF bl_dst = toQPointF(m * fromQPointF(bl));
        PkPointF br_dst = toQPointF(m * fromQPointF(br));
        PkPointF v_dst = toQPointF(m * v);
        PkPointF otherV_dst = toQPointF(m * otherV);

        PkVector<PkPointF> srcPoints;
        PkVector<PkPointF> dstPoints;

        PkPointF far1_src;
        PkPointF far2_src;
        PkPointF near1_src;
        PkPointF near2_src;

        PkPointF far1_dst;
        PkPointF far2_dst;
        PkPointF near1_dst;
        PkPointF near2_dst;

        if (m_d->function == DRAG_X_VANISHING_POINT) {

            // topLeft (far) --- topRight (near) --- vanishing
            if (kisSquareDistance(v_dst, tl_dst) > kisSquareDistance(v_dst, tr_dst)) {
                far1_src = tl;
                far2_src = bl;
                near1_src = tr;
                near2_src = br;

                far1_dst = tl_dst;
                far2_dst = bl_dst;
                near1_dst = tr_dst;
                near2_dst = br_dst;

                // topRight (far) --- topLeft (near) --- vanishing
            } else {
                far1_src = tr;
                far2_src = br;
                near1_src = tl;
                near2_src = bl;

                far1_dst = tr_dst;
                far2_dst = br_dst;
                near1_dst = tl_dst;
                near2_dst = bl_dst;
            }

        } else /* if (m_d->function == DRAG_Y_VANISHING_POINT) */{
            // topLeft (far) --- bottomLeft (near) --- vanishing
            if (kisSquareDistance(v_dst, tl_dst) > kisSquareDistance(v_dst, bl_dst)) {
                far1_src = tl;
                far2_src = tr;
                near1_src = bl;
                near2_src = br;

                far1_dst = tl_dst;
                far2_dst = tr_dst;
                near1_dst = bl_dst;
                near2_dst = br_dst;

                // bottomLeft (far) --- topLeft (near) --- vanishing
            } else {
                far1_src = bl;
                far2_src = br;
                near1_src = tl;
                near2_src = tr;

                far1_dst = bl_dst;
                far2_dst = br_dst;
                near1_dst = tl_dst;
                near2_dst = tr_dst;
            }
        }

        PkLineF l0(far1_dst, mousePos);
        PkLineF l1(far2_dst, mousePos);
        PkLineF l2(otherV_dst, near1_dst);
        l0.intersects(l2, &near1_dst);
        l1.intersects(l2, &near2_dst);

        srcPoints << far1_src;
        srcPoints << far2_src;
        srcPoints << near1_src;
        srcPoints << near2_src;

        dstPoints << far1_dst;
        dstPoints << far2_dst;
        dstPoints << near1_dst;
        dstPoints << near2_dst;

        Eigen::Matrix3f A = getTransitionMatrix(srcPoints);
        Eigen::Matrix3f B = getTransitionMatrix(dstPoints);
        Eigen::Matrix3f result = B * A.inverse();

        m_d->transformIntoArgs(result);
        break;
    }
    }

    m_d->recalculateTransformations();
}

bool KisPerspectiveTransformStrategy::endPrimaryAction()
{
    bool shouldSave = !m_d->imageTooBig;
    m_d->isTransforming = false;

    if (m_d->imageTooBig) {
        m_d->currentArgs = m_d->clickArgs;
        m_d->recalculateTransformations();
    }

    return shouldSave;
}

void KisPerspectiveTransformStrategy::Private::recalculateTransformations()
{
    transform = transformFromArgs();

    PkTransform viewScaleTransform = converter->imageToDocumentTransform() * converter->documentToFlakeTransform();
    handlesTransform = transform * viewScaleTransform;

    PkTransform tl = PkTransform::fromTranslate(transaction.originalTopLeft().x(), transaction.originalTopLeft().y());
    paintingTransform = tl.inverted() * q->thumbToImageTransform() * tl * transform * viewScaleTransform;
    paintingOffset = transaction.originalTopLeft();

    // check whether image is too big to be displayed or not
    const qreal maxScale = 20.0;

    imageTooBig = false;

    if (qAbs(currentArgs.scaleX()) > maxScale ||
        qAbs(currentArgs.scaleY()) > maxScale) {

        imageTooBig = true;

    } else {
        PkVector<PkPointF> points;
        points << transaction.originalRect().topLeft();
        points << transaction.originalRect().topRight();
        points << transaction.originalRect().bottomRight();
        points << transaction.originalRect().bottomLeft();

        for (int i = 0; i < points.size(); i++) {
            points[i] = transform.map(points[i]);
        }

        for (int i = 0; i < points.size(); i++) {
            const PkPointF &pt = points[i];
            const PkPointF &prev = points[(i - 1 + 4) % 4];
            const PkPointF &next = points[(i + 1) % 4];
            const PkPointF &other = points[(i + 2) % 4];

            PkLineF l1(pt, other);
            PkLineF l2(prev, next);

            PkPointF intersection;
            l1.intersects(l2, &intersection);

            qreal maxDistance = kisSquareDistance(pt, other);

            if (kisSquareDistance(pt, intersection) > maxDistance ||
                kisSquareDistance(other, intersection) > maxDistance) {

                imageTooBig = true;
                break;
            }

            const qreal thresholdDistance = 0.02 * l2.length();

            if (kisDistanceToLine(pt, l2) < thresholdDistance) {
                imageTooBig = true;
                break;
            }
        }
    }

    // recalculate cached handles position
    recalculateTransformedHandles();

    q->requestShowImageTooBig(imageTooBig);
    q->requestImageRecalculation();
}
