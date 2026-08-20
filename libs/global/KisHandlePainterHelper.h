#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QColor>
/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISHANDLEPAINTERHELPER_H
#define KISHANDLEPAINTERHELPER_H

#include "kritaglobal_export.h"
#include "kis_algebra_2d.h"


#include <KisHandleStyle.h>
class PkPainter;
class KoShape;
class KoViewConverter;

/**
 * @brief The KisHandlePainterHelper class is a special helper for
 *        painting handles around objects. It ensures the handles are painted
 *        with the same size and line width whatever transformation is setup
 *        in the painter. The handles will also be rotated/skewed if the object
 *        itself has these transformations.
 *
 *        On construction it resets PkPainter transformation and on destruction
 *        recovers it back.
 *
 * Please consider using KoShape::createHandlePainterHelper instead of direct
 * construction of the helper. This factory method will also apply the
 * transformations needed for a shape.
 */

class KRITAGLOBAL_EXPORT KisHandlePainterHelper
{
public:

    /**
     * Creates the helper, initializes all the internal transformations and
     * *resets* the transformation of the painter.
     */
    KisHandlePainterHelper(PkPainter *_painter, qreal handleRadius = 0.0, int decorationThickness = 1);

    /**
     * Creates the helper, initializes all the internal transformations and
     * *resets* the transformation of the painter. This override also adjusts the
     * transformation of the painter into the coordinate system of the shape
     */
    KisHandlePainterHelper(PkPainter *_painter, const PkTransform &originalPainterTransform, qreal handleRadius, int decorationThickness = 1);

    /**
     * Move c-tor. Used to create and return the helper from functions by-value.
     */
    KisHandlePainterHelper(KisHandlePainterHelper &&rhs);
    KisHandlePainterHelper(KisHandlePainterHelper &rhs) = delete;

    /**
     * Restores the transformation of the painter
     */
    ~KisHandlePainterHelper();

    /**
     * Sets style used for painting the handles. Please use static methods of
     * KisHandleStyle to select predefined styles.
     */
    void setHandleStyle(const KisHandleStyle &style);

    /**
     * Draws a handle rect with a custom \p radius at position \p center
     */
    void drawHandleRect(const PkPointF &center, qreal radius);
    void drawHandleRect(const PkPointF &center, qreal radius, PkPoint offset);
    void fillHandleRect(const PkPointF &center, qreal radius, PkColor fillColor, PkPoint offset);

    /**
     * Draws a handle circle with a custom \p radius at position \p center
     */
    void drawHandleCircle(const PkPointF &center, qreal radius);

    /**
     * Optimized version of the drawing method for drawing handles of
     * predefined size
     */
    void drawHandleRect(const PkPointF &center);

    /**
     * Optimized version of the drawing method for drawing handles of
     * predefined size
     */
    void drawHandleCircle(const PkPointF &center);

    /**
     * Optimized version of the drawing method for drawing handles of
     * predefined size
     */
    void drawHandleSmallCircle(const PkPointF &center);

    /**
     * Draws a line in the style of a handle with \p width indicating the thickness.
     */
    void drawHandleLine(const PkLineF &line, qreal width = 1.0, PkVector<qreal> dashPattern = {}, qreal dashOffset = 0.0);

    /**
     * Draw a rotated handle representing the gradient handle
     */
    void drawGradientHandle(const PkPointF &center, qreal radius);

    /**
     * Draw a rotated handle representing the gradient handle
     */
    void drawGradientHandle(const PkPointF &center);

    /**
     * Draw a special handle representing the center of the gradient
     */
    void drawGradientCrossHandle(const PkPointF &center, qreal radius);

    /**
     * Draw an arrow representing gradient position
     */
    void drawGradientArrow(const PkPointF &start, const PkPointF &end, qreal radius);

    /**
     * Draw a line showing the bounding box of the selection
     */
    void drawRubberLine(const PkPolygonF &poly);

    /**
     * Draw a line connecting two points
     */
    void drawConnectionLine(const PkLineF &line);

    /**
     * Draw a line connecting two points
     */
    void drawConnectionLine(const PkPointF &p1, const PkPointF &p2);

    /**
     * Draw an arbitrary path
     */
    void drawPath(const PkPainterPath &path);

    /**
     * Draw an a given pixmap on the UI
     */
    void drawPixmap(const PkPixmap &pixmap, PkPointF position, int size, PkRectF sourceRect);

private:

    /**
     * Draw a single arrow with the tip at position \p pos, directed from \p from,
     * of size \p radius.
     */
    void drawArrow(const PkPointF &pos, const PkPointF &from, qreal radius);

    void init();

private:
    PkPainter *m_painter;
    PkTransform m_originalPainterTransform;
    PkTransform m_painterTransform;
    qreal m_handleRadius;
    int m_decorationThickness;
    KisAlgebra2D::DecomposedMatrix m_decomposedMatrix;
    PkTransform m_handleTransform;
    PkPolygonF m_handlePolygon;
    KisHandleStyle m_handleStyle;
};

#endif // KISHANDLEPAINTERHELPER_H
