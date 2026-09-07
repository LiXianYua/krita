/*
 *  SPDX-FileCopyrightText: 2025 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KOSVGTEXTSHAPEOUTLINEHELPER_H
#define KOSVGTEXTSHAPEOUTLINEHELPER_H

#include <KoCanvasBase.h>
#include <KoViewConverter.h>

#include <QPainter>
#include <PkPoint.h>
#include <PkScopedPointer.h>

#include <kritaflake_export.h>

class KoSvgTextShape;
class PkPainter;
/**
 * @brief The KoSvgTextShapeOutlineHelper class
 * helper class that draws the text outlines and
 * contour mode button of the KoSvgTextTool.
 */
class KRITAFLAKE_EXPORT KoSvgTextShapeOutlineHelper
{
public:
    KoSvgTextShapeOutlineHelper(KoCanvasBase *canvas);
    ~KoSvgTextShapeOutlineHelper();

    void paint(QPainter *painter, const KoViewConverter &converter);
    void paint(PkPainter *painter, const KoViewConverter &converter);

    /**
     * @brief decorationRect
     * @return the current rect necessary.
     */
    PkRectF decorationRect();

    /**
     * @brief setDrawBoundingRect
     * Whether to draw the bounding rect of the shapes.
     * @param enable
     */
    void setDrawBoundingRect(bool enable);
    bool drawBoundingRect() const;

    /**
     * @brief setDrawTextWrappingArea
     * draw the wrapping area. The wrapping area is computed from
     * the shapes as well as padding and margin.
     * This also draws connection lines.
     */
    void setDrawTextWrappingArea(bool enable);
    bool drawTextWrappingArea() const;

    /**
     * @brief setDrawShapeOutlines
     * Draw the shape outlines instead of only the rect.
     */
    void setDrawShapeOutlines(bool enable);
    bool drawShapeOutlines() const;

    void setHandleRadius(int radius);
    void setDecorationThickness(int thickness);

    KoSvgTextShape *contourModeButtonHovered(const PkPointF &point);

    /**
     * @brief updateTextContourMode
     * This checks if the text contour mode still has shapes,
     * and otherwise switches out of this mode.
     * @return whether the mode was updated.
     */
    bool updateTextContourMode();

    void toggleTextContourMode(KoSvgTextShape *shape);

    void setTextAreasHovered(bool enabled);

private:
    void paintTextShape(QPainter *painter, const KoViewConverter &converter,
                        const QPalette &pal, KoSvgTextShape *text,
                        bool contourModeActive = false);
    void paintTextShape(PkPainter *painter, const KoViewConverter &converter,
                        KoSvgTextShape *text, bool contourModeActive = false);
    struct Private;
    PkScopedPointer<Private> d;
};

#endif // KOSVGTEXTSHAPEOUTLINEHELPER_H
