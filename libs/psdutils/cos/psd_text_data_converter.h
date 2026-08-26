/*
 *  SPDX-FileCopyrightText: 2025 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PSDTEXTDATAPARSER_H
#define PSDTEXTDATAPARSER_H

#include <memory>
#include <PkColor.h>
#include <PkMap.h>
#include <PkRect.h>
#include <PkStringList.h>
#include <PkTransform.h>
#include <PkVariant.h>
#include <PkVector.h>
#include <PkXmlElement.h>
#include "kritapsdutils_export.h"

class KoSvgTextShape;
class KoColorSpace;
class KoShape;
struct KoCSSFontInfo;
class KoSvgTextProperties;
class PkXmlElement;

/**
 * @brief The PsdTextDataConverter class
 *
 * This class handles converting PSD text engine data to actual SVG.
 */
class KRITAPSDUTILS_EXPORT PsdTextDataConverter
{
public:
    PsdTextDataConverter();
    ~PsdTextDataConverter();

    bool convertPSDTextEngineDataToSVG(const PkVariantHash tySh,
                                       const PkVariantHash txt2,
                                       const KoColorSpace *imageCs,
                                       const int textIndex,
                                       PkString *svgText,
                                       PkString *svgStyles,
                                       PkPointF &offset,
                                       bool &offsetByAscent,
                                       bool &isHorizontal,
                                       PkTransform scaleToPt = PkTransform());
    bool convertToPSDTextEngineData(const PkString &svgText,
                                    PkRectF &boundingBox,
                                    const PkList<KoShape *> &shapesInside,
                                    PkVariantHash &txt2,
                                    int &textIndex,
                                    PkString &textTotal,
                                    bool &isHorizontal,
                                    PkTransform scaleToPx = PkTransform());

    /**
     * A list of errors happened during loading the user's text
     */
    PkStringList errors() const;
    /**
     * A list of warnings produced during loading the user's text
     */
    PkStringList warnings() const;
private:

    PkColor colorFromPSDStyleSheet(PkVariantHash color, const KoColorSpace *imageCs);
    PkString stylesForPSDStyleSheet(PkString &lang, PkVariantHash PSDStyleSheet, PkMap<int, KoCSSFontInfo> fontNames, PkTransform scale, const KoColorSpace *imageCs);
    PkString stylesForPSDParagraphSheet(PkVariantHash PSDParagraphSheet, PkString &lang, PkMap<int, KoCSSFontInfo> fontNames, PkTransform scaleToPt, const KoColorSpace *imageCs);

    PkVariantHash styleToPSDStylesheet(const PkMap<PkString, PkString> cssStyles, PkVariantHash parentStyle, PkTransform scaleToPx);
    PkVariantHash gatherParagraphStyle(PkXmlElement el, PkVariantHash defaultProperties, bool &isHorizontal, PkString *inlineSize, PkTransform scaleToPx);
    void gatherFonts(const PkMap<PkString, PkString> cssStyles, const PkString text, PkVariantList &fontSet,
                     PkVector<int> &lengths, PkVector<int> &fontIndices);
    void gatherStyles(PkXmlElement el, PkString &text, PkVariantHash parentStyle, PkMap<PkString, PkString> parentCssStyles, PkVariantList &styles, PkVariantList &fontSet, PkTransform scaleToPx);
    struct Private;
    const std::unique_ptr<Private> d;
};

#endif // PSDTEXTDATAPARSER_H
