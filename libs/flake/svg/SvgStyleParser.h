/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2002-2003, 2005 Rob Buis <buis@kde.org>
 * SPDX-FileCopyrightText: 2005-2006 Tim Beaulen <tbscope@gmail.com>
 * SPDX-FileCopyrightText: 2005, 2007-2009 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef SVGSTYLEPARSER_H
#define SVGSTYLEPARSER_H
#include <PkColor.h>
#include <PkGlobal.h>
#include <PkGradient.h>
#include <PkMap.h>
#include <PkString.h>
#include <PkStringList.h>
#include <PkXmlElement.h>


#include "kritaflake_export.h"
#include <pk/container/PkMap.h>
#include <pk/container/PkStringList.h>
#include <pk/xml/PkXmlElement.h>
#include <pk/color/PkColor.h>

#include <utility>

typedef PkMap<PkString, PkString> SvgStyles;

class SvgLoadingContext;
class SvgGraphicsContext;

class KRITAFLAKE_EXPORT SvgStyleParser
{
public:
    explicit SvgStyleParser(SvgLoadingContext &context);
    ~SvgStyleParser();

    /// Parses specified style attributes
    void parseStyle(const SvgStyles &styles, const bool inheritByDefault = false);

    /// Parses font attributes
    void parseFont(const SvgStyles &styles);

    /// Parses a color attribute
    bool parseColor(PkColor &, const PkString &);

    std::pair<qreal, PkColor> parseColorStop(const PkXmlElement&, SvgGraphicsContext* context, qreal& previousOffset);

    /// Parses gradient color stops
    void parseColorStops(PkGradient *, const PkXmlElement &, SvgGraphicsContext *context, const PkGradientStops &defaultStops);

    /// Creates style map from given xml element
    SvgStyles collectStyles(const PkXmlElement &);

    /// Merges two style elements, returning the merged style
    SvgStyles mergeStyles(const SvgStyles &, const SvgStyles &);

    /// Merges two style elements, returning the merged style
    SvgStyles mergeStyles(const PkXmlElement &, const PkXmlElement &);

    SvgStyles parseOneCssStyle(const PkString &style, const PkStringList &interestingAttributes);
private:

    /// Parses a single style attribute
    void parsePA(SvgGraphicsContext *, const PkString &, const PkString &);

    /// Returns inherited attribute value for specified element
    PkString inheritedAttribute(const PkString &attributeName, const PkXmlElement &e);

    class Private;
    Private * const d;
};

#endif // SVGSTYLEPARSER_H
