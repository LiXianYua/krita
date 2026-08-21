/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2003, 2005 Rob Buis <buis@kde.org>
 * SPDX-FileCopyrightText: 2007, 2009 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef SVGGRAPHICCONTEXT_H
#define SVGGRAPHICCONTEXT_H

#include <PkXmlCompat.h>

#include <pk/color/PkColor.h>

#include "kritaflake_export.h"
#include <KoShapeStroke.h>
#include <text/KoSvgTextProperties.h>

class KRITAFLAKE_EXPORT SvgGraphicsContext
{
public:
    // Fill/stroke styles
    enum StyleType {
        None,     ///< no style
        Solid,    ///< solid style
        Complex,   ///< gradient or pattern style
        Inherit
    };

    SvgGraphicsContext();
    SvgGraphicsContext(const SvgGraphicsContext &gc);

    void workaroundClearInheritedFillProperties();

    StyleType     fillType  {Solid};  ///< the current fill type
    Qt::FillRule  fillRule  {Qt::WindingFill};  ///< the current fill rule
    PkColor        fillColor {PkColor(Qt::black)}; ///< the current fill color. Default is black fill as per svg spec
    PkString       fillId;    ///< the current fill id (used for gradient/pattern fills)

    StyleType     strokeType {None};///< the current stroke type
    PkString       strokeId;  ///< the current stroke id (used for gradient strokes)
    KoShapeStrokeSP stroke;    ///< the current stroke

    PkString filterId;       ///< the current filter id
    PkString clipPathId;     ///< the current clip path id
    PkString clipMaskId;     ///< the current clip mask id
    Qt::FillRule clipRule {Qt::WindingFill};  ///< the current clip rule
    qreal opacity {1.0};    ///< the shapes opacity

    PkTransform matrix;      ///< the current transformation matrix
    PkColor  currentColor {Qt::black};   ///< the current color
    PkString xmlBaseDir;     ///< the current base directory (used for loading external content)
    bool preserveWhitespace {false}; ///< preserve whitespace in element text

    PkRectF currentBoundingBox; ///< the current bound box used for bounding box units
    bool   forcePercentage {false}; ///< force parsing coordinates/length as percentages of currentBoundbox
    PkTransform viewboxTransform; ///< view box transformation

    bool display {true};           ///< controls display of shape
    bool visible {true};           ///< controls visibility of the shape (inherited)
    bool isResolutionFrame {false};
    qreal pixelsPerInch {72.0};    ///< controls the resolution of the image raster

    PkString markerStartId;
    PkString markerMidId;
    PkString markerEndId;

    bool autoFillMarkers {false};

    KoSvgTextProperties textProperties; ///< Stores textProperties
    PkString shapeInsideValue; ///< String of value shape-inside, will be parsed later.
    PkString shapeSubtractValue; ///< String of value shape-subtract, will be parsed later.

    PkString paintOrder; ///< String list indicating paint order;
private:
    SvgGraphicsContext& operator=(const SvgGraphicsContext &gc) = default; ///< used by copy constructor, shouldn't be public
};

#endif // SVGGRAPHICCONTEXT_H
