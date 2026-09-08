/*
 * SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KOSVGTEXTSHAPEMARKUPCONVERTER_H
#define KOSVGTEXTSHAPEMARKUPCONVERTER_H

#include "kritaflake_export.h"

#include <PkScopedPointer.h>
#include <PkString.h>
#include <PkStringList.h>

class PkRectF;
class KoSvgTextShape;

/**
 * Converts a KoSvgTextShape to and from its editable SVG/HTML markup.
 *
 * The editable SVG is intentionally stripped and is not the same stream that
 * is stored in a .kra file. HTML import accepts the Qt 5.15 rich-text subset
 * historically produced by the SVG text editor, but conversion is Pk-native.
 */
class KRITAFLAKE_EXPORT KoSvgTextShapeMarkupConverter
{
public:
    explicit KoSvgTextShapeMarkupConverter(KoSvgTextShape *shape);
    ~KoSvgTextShapeMarkupConverter();

    bool convertToSvg(PkString *svgText, PkString *stylesText);
    bool convertFromSvg(const PkString &svgText,
                        const PkString &stylesText,
                        const PkRectF &boundsInPixels,
                        qreal pixelsPerInch);

    bool convertToHtml(PkString *htmlText);
    bool convertFromHtml(const PkString &htmlText,
                         PkString *svgText,
                         PkString *styles);

    PkStringList errors() const;
    PkStringList warnings() const;

private:
    struct Private;
    const PkScopedPointer<Private> d;
};

#endif // KOSVGTEXTSHAPEMARKUPCONVERTER_H
