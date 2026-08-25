/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2009 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef SVGUTIL_H
#define SVGUTIL_H
#include <PkGlobal.h>
#include <PkPoint.h>
#include <PkRect.h>
#include <PkSize.h>
#include <PkString.h>
#include <PkStringList.h>
#include <PkTransform.h>
#include <PkXmlElement.h>


#include "kritaflake_export.h"
#include <KoSvgText.h>
#include <pk/container/PkStringList.h>
#include <pk/xml/PkXmlElement.h>

class PkString;
class PkTransform;
class KoXmlWriter;
class KoSvgTextProperties;

class SvgGraphicsContext;

class KRITAFLAKE_EXPORT SvgUtil
{
public:

    // remove later! pixels *are* user coordinates
    static double fromUserSpace(double value);
    static double toUserSpace(double value);

    static double ptToPx(SvgGraphicsContext *gc, double value);

    /// Converts given point from points to userspace units.
    static PkPointF toUserSpace(const PkPointF &point);

    /// Converts given rectangle from points to userspace units.
    static PkRectF toUserSpace(const PkRectF &rect);

    /// Converts given rectangle from points to userspace units.
    static PkSizeF toUserSpace(const PkSizeF &size);

    /**
     * Parses the given float percentage.
     * @param value the input number containing float percentage (0..1)
     * @return the percentage string (0%..100%), not normalized
     */
    static PkString toPercentage(qreal value);

    /**
     * Parses the given string containing a percentage.
     * @param s the input string containing the percentage, float (0..1) or integer (0%..100%)
     * @param ok optional failure indicator
     * @return the float percentage (0..1), not normalized
     */
    static double fromPercentage(PkString s, bool *ok=nullptr);

    /**
     * Converts position from objectBoundingBox units to userSpace units.
     */
    static PkPointF objectToUserSpace(const PkPointF &position, const PkRectF &objectBound);

    /**
     * Converts size from objectBoundingBox units to userSpace units.
     */
    static PkSizeF objectToUserSpace(const PkSizeF &size, const PkRectF &objectBound);

    /**
     * Converts position from userSpace units to objectBoundingBox units.
     */
    static PkPointF userSpaceToObject(const PkPointF &position, const PkRectF &objectBound);

    /**
     * Converts size from userSpace units to objectBoundingBox units.
     */
    static PkSizeF userSpaceToObject(const PkSizeF &size, const PkRectF &objectBound);

    /// Converts specified transformation to a string
    static PkString transformToString(const PkTransform &transform);

    /// Writes a \p transform as an attribute \p name iff the transform is not empty
    static void writeTransformAttributeLazy(const PkString &name, const PkTransform &transform, KoXmlWriter &shapeWriter);

    /// Parses a viewbox attribute into an rectangle
    static bool parseViewBox(const PkXmlElement &e, const PkRectF &elementBounds, PkRectF *_viewRect, PkTransform *_viewTransform);

    struct PreserveAspectRatioParser;
    static void parseAspectRatio(const PreserveAspectRatioParser &p, const PkRectF &elementBounds, const PkRectF &viewRect, PkTransform *_viewTransform);

    /// Parses a length attribute
    static qreal parseUnit(SvgGraphicsContext *gc,
                           const KoSvgTextProperties &resolved,
                           const PkString &unit,
                           bool horiz = false,
                           bool vert = false,
                           const PkRectF &bbox = PkRectF());
    /// Parse length attribute into a struct, always resolving the percentage to viewport
    static KoSvgText::CssLengthPercentage parseUnitStruct(SvgGraphicsContext *gc,
                           const PkString &unit,
                           bool horiz = false,
                           bool vert = false,
                           const PkRectF &bbox = PkRectF());

    /// Unit structs for text do not need the percentage to be resolved to viewport in most cases.
    static KoSvgText::CssLengthPercentage parseTextUnitStruct(SvgGraphicsContext *gc, const PkString &unit);

    /// Parse length attribute into struct.
    static KoSvgText::CssLengthPercentage parseUnitStructImpl(SvgGraphicsContext *gc,
                                                          const PkString &unit,
                                                          bool horiz = false,
                                                          bool vert = false,
                                                          const PkRectF &bbox = PkRectF(),
                                                          bool percentageViewBox = false);

    /// parses a length attribute in x-direction
    static qreal parseUnitX(SvgGraphicsContext *gc, const KoSvgTextProperties &resolved, const PkString &unit);

    /// parses a length attribute in y-direction
    static qreal parseUnitY(SvgGraphicsContext *gc, const KoSvgTextProperties &resolved, const PkString &unit);

    /// parses a length attribute in xy-direction
    static qreal parseUnitXY(SvgGraphicsContext *gc, const KoSvgTextProperties &resolved, const PkString &unit);

    /// parses angle, result in *radians*!
    static qreal parseUnitAngular(SvgGraphicsContext *gc, const PkString &unit);

    /// parses the number into parameter number
    static const char * parseNumber(const char *ptr, qreal &number);

    static qreal parseNumber(const PkString &string);

    static PkString mapExtendedShapeTag(const PkString &tagName, const PkXmlElement &element);

    static PkStringList simplifyList(const PkString &str);

    struct KRITAFLAKE_EXPORT PreserveAspectRatioParser
    {
        PreserveAspectRatioParser(const PkString &str);

        enum Alignment {
            Min,
            Middle,
            Max
        };

        bool defer = false;
        Qt::AspectRatioMode mode = Qt::IgnoreAspectRatio;
        Alignment xAlignment = Min;
        Alignment yAlignment = Min;

        PkPointF rectAnchorPoint(const PkRectF &rc) const;

        PkString toString() const;

    private:
        Alignment alignmentFromString(const PkString &str) const;
        PkString alignmentToString(Alignment alignment) const;
        static qreal alignedValue(qreal min, qreal max, Alignment alignment);
    };
};

#endif // SVGUTIL_H
