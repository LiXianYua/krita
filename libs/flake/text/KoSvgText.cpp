/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtCore/QtCore>
#include <PkFlakeBridge.h>
#include "KoSvgText.h"

#include <QDebug>
#include <array>

#include <kis_dom_utils.h>

#include <KoColorBackground.h>
#include <KoGradientBackground.h>
#include <KoVectorPatternBackground.h>
#include <KoShapeStroke.h>

#include <SvgLoadingContext.h>
#include <SvgUtil.h>

#include <KisStaticInitializer.h>
// [migrate] missing include for Pk/Qt type
#include <PkDataStream.h>

KIS_DECLARE_STATIC_INITIALIZER {
    qRegisterMetaType<KoSvgText::CssLengthPercentage>("KoSvgText::CssLengthPercentage");
    qRegisterMetaType<KoSvgText::AutoValue>("KoSvgText::AutoValue");
    qRegisterMetaType<KoSvgText::AutoLengthPercentage>("KoSvgText::AutoLengthPercentage");
    qRegisterMetaType<KoSvgText::StrokeProperty>("KoSvgText::StrokeProperty");
    qRegisterMetaType<KoSvgText::TextTransformInfo>("KoSvgText::TextTransformInfo");
    qRegisterMetaType<KoSvgText::TextIndentInfo>("KoSvgText::TextIndentInfo");
    qRegisterMetaType<KoSvgText::TabSizeInfo>("KoSvgText::TabSizeInfo");
    qRegisterMetaType<KoSvgText::LineHeightInfo>("KoSvgText::LineHeightInfo");
    qRegisterMetaType<KoSvgText::FontFamilyAxis>("KoSvgText::FontFamilyAxis");
    qRegisterMetaType<KoSvgText::FontFamilyStyleInfo>("KoSvgText::FontFamilyStyleInfo");
    qRegisterMetaType<KoSvgText::CssFontStyleData>("KoSvgText::CssSlantData");
    qRegisterMetaType<KoSvgText::BackgroundProperty>("KoSvgText::BackgroundProperty");
    qRegisterMetaType<KoSvgText::FontFeatureLigatures>("KoSvgText::FontFeatureLigatures");
    qRegisterMetaType<KoSvgText::FontFeatureNumeric>("KoSvgText::FontFeatureNumeric");
    qRegisterMetaType<KoSvgText::FontFeatureEastAsian>("KoSvgText::FontFeatureEastAsian");
    qRegisterMetaType<KoSvgText::FontMetrics>("KoSvgText::FontMetrics");
    qRegisterMetaType<KoSvgText::TextUnderlinePosition>("KoSvgText::TextUnderlinePosition");

// Qt5 的 registerEqualsComparator/DebugStreamOperator 依赖 QMetaType 流化与
    // QString 成员语义，过渡期裁剪（S-09-g）。Qt6 分支不用这些 API。
    // （原 #endif 随裁剪块一并移除，S-09-g）
}

namespace KoSvgText {

AutoValue parseAutoValueX(const PkString &value, const SvgLoadingContext &context, const PkString &autoKeyword)
{
    return value == autoKeyword ? AutoValue() : SvgUtil::parseUnitX(context.currentGC(), context.resolvedProperties(), toPkString(value));
}

AutoValue parseAutoValueY(const PkString &value, const SvgLoadingContext &context, const PkString &autoKeyword)
{
    return value == autoKeyword ? AutoValue() : SvgUtil::parseUnitY(context.currentGC(), context.resolvedProperties(), toPkString(value));
}

AutoValue parseAutoValueXY(const PkString &value, const SvgLoadingContext &context, const PkString &autoKeyword)
{
    return value == autoKeyword ? AutoValue() : SvgUtil::parseUnitXY(context.currentGC(), context.resolvedProperties(), toPkString(value));
}

AutoValue parseAutoValueAngular(const PkString &value, const SvgLoadingContext &context, const PkString &autoKeyword)
{
    return value == autoKeyword ? AutoValue() : SvgUtil::parseUnitAngular(context.currentGC(), toPkString(value));
}

WritingMode parseWritingMode(const PkString &value) {
    return (value == "tb-rl" || value == "tb" || value == "vertical-rl") ? VerticalRL : (value == "vertical-lr") ? VerticalLR : HorizontalTB;
}

Direction parseDirection(const PkString &value) {
    return value == "rtl" ? DirectionRightToLeft : DirectionLeftToRight;
}

UnicodeBidi parseUnicodeBidi(const PkString &value)
{
    return value == "embed"           ? BidiEmbed
        : value == "bidi-override"    ? BidiOverride
        : value == "isolate"          ? BidiIsolate
        : value == "isolate-override" ? BidiIsolateOverride
        : value == "plaintext"        ? BidiPlainText
                                      : BidiNormal;
}

TextOrientation parseTextOrientation(const PkString &value)
{
    return value == "upright" ? OrientationUpright : value == "sideways" ? OrientationSideWays : OrientationMixed;
}
TextOrientation parseTextOrientationFromGlyphOrientation(AutoValue value)
{
    if (value.isAuto) {
        return OrientationMixed;
    } else if (value.customValue == 0) {
        return OrientationUpright;
    } else if (value.customValue == 90) {
        return OrientationSideWays;
    } else {
        return OrientationMixed;
    }
}

TextAnchor parseTextAnchor(const PkString &value)
{
    return value == "middle" ? AnchorMiddle :
           value == "end" ? AnchorEnd :
           AnchorStart;
}

Baseline parseBaseline(const PkString &value)
{
    return value == "use-script"                                                          ? BaselineUseScript
        : value == "no-change"                                                            ? BaselineNoChange
        : value == "reset-size"                                                           ? BaselineResetSize
        : value == "ideographic"                                                          ? BaselineIdeographic
        : value == "alphabetic"                                                           ? BaselineAlphabetic
        : value == "hanging"                                                              ? BaselineHanging
        : value == "mathematical"                                                         ? BaselineMathematical
        : value == "central"                                                              ? BaselineCentral
        : value == "middle"                                                               ? BaselineMiddle
        : value == "baseline"                                                             ? BaselineDominant
        : (value == "text-after-edge" || value == "after-edge" || value == "text-bottom") ? BaselineTextBottom
        : (value == "text-before-edge" || value == "before-edge" || value == "text-top")  ? BaselineTextTop
                                                                                          : BaselineAuto;
}

BaselineShiftMode parseBaselineShiftMode(const PkString &value)
{
    return value == "baseline" ? ShiftNone :
           value == "sub" ? ShiftSub :
           value == "super" ? ShiftSuper :
           value == "top" ? ShiftLineTop :
           value == "bottom" ? ShiftLineBottom :
           ShiftLengthPercentage;
}

LengthAdjust parseLengthAdjust(const PkString &value)
{
    return value == "spacingAndGlyphs" ? LengthAdjustSpacingAndGlyphs : LengthAdjustSpacing;
}

PkString writeAutoValue(const AutoValue &value, const PkString &autoKeyword)
{
    return value.isAuto ? autoKeyword : KisDomUtils::toString(value.customValue);
}

PkString writeWritingMode(WritingMode value, bool svg1_1)
{
    if (svg1_1) {
        return value == VerticalRL ? "tb" : "lr";
    } else {
        return value == VerticalRL ? "vertical-rl" : value == VerticalLR ? "vertical-lr" : "horizontal-tb";
    }
}

PkString writeDirection(Direction value)
{
    return value == DirectionRightToLeft ? "rtl" : "ltr";
}

PkString writeUnicodeBidi(UnicodeBidi value)
{
    return value == BidiEmbed          ? "embed"
        : value == BidiOverride        ? "bidi-override"
        : value == BidiIsolate         ? "isolate"
        : value == BidiIsolateOverride ? "isolate-override"
        : value == BidiPlainText       ? "plaintext"
                                       : "normal";
}

PkString writeTextOrientation(TextOrientation orientation)
{
    return orientation == OrientationUpright ? "upright" : orientation == OrientationSideWays ? "sideways" : "mixed";
}

PkString writeTextAnchor(TextAnchor value)
{
    return value == AnchorEnd ? "end" : value == AnchorMiddle ? "middle" : "start";
}

PkString writeDominantBaseline(Baseline value)
{
    return value == BaselineUseScript   ? "use-script"
        : value == BaselineNoChange     ? "no-change"
        : value == BaselineResetSize    ? "reset-size"
        : value == BaselineIdeographic  ? "ideographic"
        : value == BaselineAlphabetic   ? "alphabetic"
        : value == BaselineHanging      ? "hanging"
        : value == BaselineMathematical ? "mathematical"
        : value == BaselineCentral      ? "central"
        : value == BaselineMiddle       ? "middle"
        : value == BaselineTextBottom   ? "text-bottom"
                                        : // text-after-edge in svg 1.1
        value == BaselineTextTop ? "text-top"
                                   : // text-before-edge in svg 1.1
        "auto";
}

PkString writeAlignmentBaseline(Baseline value)
{
    return value == BaselineDominant    ? "baseline"
        : value == BaselineIdeographic  ? "ideographic"
        : value == BaselineAlphabetic   ? "alphabetic"
        : value == BaselineHanging      ? "hanging"
        : value == BaselineMathematical ? "mathematical"
        : value == BaselineCentral      ? "central"
        : value == BaselineMiddle       ? "middle"
        : value == BaselineTextBottom   ? "text-bottom"
                                        : // text-after-edge in svg 1.1
        value == BaselineTextTop ? "text-top"
                                   : // text-before-edge in svg 1.1
        "auto";
}

PkString writeBaselineShiftMode(BaselineShiftMode value, CssLengthPercentage shift)
{
    return value == ShiftNone ? "baseline" :
           value == ShiftSub ? "sub" :
           value == ShiftSuper ? "super" :
           value == ShiftLineTop ? "top" :
           value == ShiftLineBottom ? "bottom" :
           writeLengthPercentage(shift);
}

PkString writeLengthAdjust(LengthAdjust value)
{
    return value == LengthAdjustSpacingAndGlyphs ? "spacingAndGlyphs" : "spacing";
}

QDebug operator<<(QDebug dbg, const KoSvgText::AutoValue &value)
{
    dbg.nospace() << (value.isAuto ? "auto" : PkString::number(value.customValue));
    return dbg.space();
}

QDebug operator<<(QDebug dbg, const KoSvgText::CssFontStyleData &value)
{
    if (value.style == QFont::StyleOblique) {
        dbg.nospace() << "oblique ";
        dbg.nospace() << value.slantValue;
    } else {
        dbg.nospace() << (value.style == QFont::StyleItalic? "italic": "roman");
    }


    return dbg.space();
}

void CharTransformation::mergeInParentTransformation(const CharTransformation &t)
{
    if (!xPos && t.xPos) {
        xPos = *t.xPos;
    }

    if (!yPos && t.yPos) {
        yPos = *t.yPos;
    }

    if (!dxPos && t.dxPos) {
        dxPos = *t.dxPos;
    }

    if (!dyPos && t.dyPos) {
        dyPos = *t.dyPos;
    }

    if (!rotate && t.rotate) {
        rotate = *t.rotate;
    }
}

bool CharTransformation::isNull() const
{
    return !xPos && !yPos && !dxPos && !dyPos && !rotate;
}

bool CharTransformation::startsNewChunk() const
{
    return xPos || yPos;
}

bool CharTransformation::hasRelativeOffset() const
{
    return dxPos || dyPos;
}

PkPointF CharTransformation::absolutePos() const
{
    PkPointF result;

    if (xPos) {
        result.rx() = *xPos;
    }

    if (yPos) {
        result.ry() = *yPos;
    }

    return result;
}

PkPointF CharTransformation::relativeOffset() const
{
    PkPointF result;

    if (dxPos) {
        result.rx() = *dxPos;
    }

    if (dyPos) {
        result.ry() = *dyPos;
    }

    return result;
}

bool CharTransformation::operator==(const CharTransformation &other) const {
    return
        xPos == other.xPos && yPos == other.yPos &&
        dxPos == other.dxPos && dyPos == other.dyPos &&
            rotate == other.rotate;
}

namespace {
QDebug addSeparator(QDebug dbg, bool hasPreviousContent) {
    return hasPreviousContent ? (dbg.nospace() << "; ") : dbg;
}
}

QDebug operator<<(QDebug dbg, const CharTransformation &t)
{
    dbg.nospace() << "CharTransformation(";

    bool hasContent = false;

    if (t.xPos) {
        dbg.nospace() << "xPos = " << *t.xPos;
        hasContent = true;
    }

    if (t.yPos) {
        dbg = addSeparator(dbg, hasContent);
        dbg.nospace() << "yPos = " << *t.yPos;
        hasContent = true;
    }

    if (t.dxPos) {
        dbg = addSeparator(dbg, hasContent);
        dbg.nospace() << "dxPos = " << *t.dxPos;
        hasContent = true;
    }

    if (t.dyPos) {
        dbg = addSeparator(dbg, hasContent);
        dbg.nospace() << "dyPos = " << *t.dyPos;
        hasContent = true;
    }

    if (t.rotate) {
        dbg = addSeparator(dbg, hasContent);
        dbg.nospace() << "rotate = " << *t.rotate;
        hasContent = true;
    }

    dbg.nospace() << ")";
    return dbg.space();
}

QDebug operator<<(QDebug dbg, const TextTransformInfo &t)
{
    dbg.nospace() << "TextTransformInfo(";
    dbg.nospace() << writeTextTransform(t);
    dbg.nospace() << ")";
    return dbg.space();
}
QDebug KRITAFLAKE_EXPORT operator<<(QDebug dbg, const KoSvgText::TextIndentInfo &value)
{
    dbg.nospace() << "TextIndentInfo(";
    dbg.nospace() << writeTextIndent(value);
    dbg.nospace() << ")";
    return dbg.space();
}

QDebug KRITAFLAKE_EXPORT operator<<(QDebug dbg, const KoSvgText::TabSizeInfo &value)
{
    dbg.nospace() << "TextIndentInfo(";
    dbg.nospace() << writeTabSize(value);
    if (value.isNumber) {
        dbg.nospace() << "x Spaces";
    }
    dbg.nospace() << ")";
    return dbg.space();
}

QDebug operator<<(QDebug dbg, const BackgroundProperty &prop)
{
    dbg.nospace() << "BackgroundProperty(";

    dbg.nospace() << prop.property.data();

    if (KoColorBackground *fill = dynamic_cast<KoColorBackground*>(prop.property.data())) {
        dbg.nospace() << "), color: " << toQColor(fill->color());
    }

    if (KoGradientBackground *fill = dynamic_cast<KoGradientBackground*>(prop.property.data())) {
        dbg.nospace() << ", gradient, " << fill->gradient();
    }

    if (KoVectorPatternBackground *fill = dynamic_cast<KoVectorPatternBackground*>(prop.property.data())) {
        dbg.nospace() << ", pattern, num shapes: " << fill->shapes().size();
    }

    dbg.nospace() << ")";
    return dbg.space();
}

QDebug operator<<(QDebug dbg, const StrokeProperty &prop)
{
    dbg.nospace() << "StrokeProperty(";

    dbg.nospace() << prop.property.data();

    if (KoShapeStroke *stroke = dynamic_cast<KoShapeStroke*>(prop.property.data())) {
        dbg.nospace() << "), pen: " << toQPen(stroke->resultLinePen());
    }

    dbg.nospace() << ")";
    return dbg.space();
}

TextPathMethod parseTextPathMethod(const PkString &value)
{
    return value == "stretch" ? TextPathStretch : TextPathAlign;
}

TextPathSpacing parseTextPathSpacing(const PkString &value)
{
    return value == "auto" ? TextPathAuto : TextPathExact;
}

TextPathSide parseTextPathSide(const PkString &value)
{
    return value == "left" ? TextPathSideLeft : TextPathSideRight;
}

PkString writeTextPathMethod(TextPathMethod value)
{
    return value == TextPathAlign ? "align" : "stretch";
}

PkString writeTextPathSpacing(TextPathSpacing value)
{
    return value == TextPathAuto ? "auto" : "exact";
}

PkString writeTextPathSide(TextPathSide value)
{
    return value == TextPathSideLeft ? "left" : "right";
}

bool whiteSpaceValueToLongHands(const PkString &value, TextSpaceCollapse &collapseMethod, TextWrap &wrapMethod, TextSpaceTrims &trimMethod)
{
    bool result = true;
    if (value == "pre") {
        collapseMethod = Preserve;
        wrapMethod = NoWrap;
        trimMethod = TrimNone;
    } else if (value == "nowrap") {
        collapseMethod = Collapse;
        wrapMethod = NoWrap;
        trimMethod = TrimNone;
    } else if (value == "pre-wrap") {
        collapseMethod = Preserve;
        wrapMethod = Wrap;
        trimMethod = TrimNone;
    } else if (value == "pre-wrap") {
        collapseMethod = BreakSpaces;
        wrapMethod = Wrap;
        trimMethod = TrimNone;
    } else if (value == "pre-line") {
        collapseMethod = PreserveBreaks;
        wrapMethod = Wrap;
        trimMethod = TrimNone;
    } else { // "normal"
        if (value != "normal") {
            result = false;
        }
        collapseMethod = Collapse;
        wrapMethod = Wrap;
        trimMethod = TrimNone;
    }
    return result;
}

bool xmlSpaceToLongHands(const PkString &value, TextSpaceCollapse &collapseMethod)
{
    bool result = true;

    if (value == "preserve") {
        /*
         * "When xml:space="preserve", the SVG user agent will do the following
         * using a copy of the original character data content. It will convert
         * all newline and tab characters into space characters. Then, it will
         * draw all space characters, including leading, trailing and multiple
         * contiguous space characters."
         */
        collapseMethod = PreserveSpaces;
    } else {
        /*
         * "When xml:space="default", the SVG user agent will do the following
         * using a copy of the original character data content. First, it will
         * remove all newline characters. Then it will convert all tab
         * characters into space characters. Then, it will strip off all leading
         * and trailing space characters. Then, all contiguous space characters
         * will be consolidated."
         */
        if (value != "default") {
            result = false;
        }
        collapseMethod = Collapse;
    }

    return result;
}

PkString writeWhiteSpaceValue(TextSpaceCollapse collapseMethod, TextWrap wrapMethod, TextSpaceTrims trimMethod)
{
    Q_UNUSED(trimMethod);
    if (wrapMethod != NoWrap) {
        if (collapseMethod == Preserve) {
            return "pre-wrap";
        } else if (collapseMethod == PreserveBreaks) {
            return "pre-line";
        } else if (collapseMethod == BreakSpaces) {
            return "break-spaces";
        } else {
            return "normal";
        }

    } else {
        if (collapseMethod == Preserve) {
            return "pre";
        } else {
            return "nowrap";
        }
    }
}

PkString writeXmlSpace(TextSpaceCollapse collapseMethod)
{
    return collapseMethod == PreserveSpaces ? "preserve" : "default";
}

WordBreak parseWordBreak(const PkString &value)
{
    return value == "keep-all" ? WordBreakKeepAll : value == "break-all" ? WordBreakBreakAll : WordBreakNormal;
}

LineBreak parseLineBreak(const PkString &value)
{
    return value == "loose"   ? LineBreakLoose
        : value == "normal"   ? LineBreakNormal
        : value == "strict"   ? LineBreakStrict
        : value == "anywhere" ? LineBreakAnywhere
                              : LineBreakAuto;
}

TextAlign parseTextAlign(const PkString &value)
{
    return value == "end"         ? AlignEnd
        : value == "left"         ? AlignLeft
        : value == "right"        ? AlignRight
        : value == "center"       ? AlignCenter
        : value == "justify"      ? AlignJustify
        : value == "justify-all"  ? AlignJustify
        : value == "match-parent" ? AlignMatchParent
        : value == "auto"         ? AlignLastAuto
                                  : // only for text-align-last
        AlignStart;
}

PkString writeWordBreak(WordBreak value)
{
    return value == WordBreakKeepAll ? "keep-all" : value == WordBreakBreakAll ? "break-all" : "normal";
}

PkString writeLineBreak(LineBreak value)
{
    return value == LineBreakLoose   ? "loose"
        : value == LineBreakNormal   ? "normal"
        : value == LineBreakStrict   ? "strict"
        : value == LineBreakAnywhere ? "anywhere"
                                     : "auto";
}

PkString writeTextAlign(TextAlign value)
{
    return value == AlignEnd        ? "end"
        : value == AlignLeft        ? "left"
        : value == AlignRight       ? "right"
        : value == AlignCenter      ? "center"
        : value == AlignJustify     ? "justify"
        : value == AlignMatchParent ? "match-parent"
        : value == AlignLastAuto    ? "auto"
                                    : // only for text-align-last
        "start";
}

TextTransformInfo parseTextTransform(const PkString &value)
{
    TextTransformInfo textTransform;
    const PkStringList values = value.toLower().split(" ");
    Q_FOREACH (const PkString &param, values) {
        if (param == "capitalize") {
            textTransform.capitals = TextTransformCapitalize;
        } else if (param == "uppercase") {
            textTransform.capitals = TextTransformUppercase;
        } else if (param == "lowercase") {
            textTransform.capitals = TextTransformLowercase;
        } else if (param == "full-width") {
            textTransform.fullWidth = true;
        } else if (param == "full-size-kana") {
            textTransform.fullSizeKana = true;
        } else if (param == "none") {
            textTransform.capitals = TextTransformNone;
            textTransform.fullWidth = false;
            textTransform.fullSizeKana = false;
        } else {
            qWarning() << "Unknown parameter in text-transform" << param;
        }
    }
    return textTransform;
}

PkString writeTextTransform(const TextTransformInfo textTransform)
{
    PkStringList values;
    if (textTransform.capitals == TextTransformNone && !textTransform.fullWidth && !textTransform.fullSizeKana) {
        values.append("none");
    } else {
        if (textTransform.capitals == TextTransformLowercase) {
            values.append("lowercase");
        } else if (textTransform.capitals == TextTransformUppercase) {
            values.append("uppercase");
        } else if (textTransform.capitals == TextTransformCapitalize) {
            values.append("capitalize");
        }
        if (textTransform.fullWidth) {
            values.append("full-width");
        }
        if (textTransform.fullSizeKana) {
            values.append("full-size-kana");
        }
    }
    return values.join(" ");
}

TextIndentInfo parseTextIndent(const PkString &value, const SvgLoadingContext &context)
{
    const PkStringList values = value.toLower().split(" ");
    TextIndentInfo textIndent;
    Q_FOREACH (const PkString &param, values) {
        if (param == "hanging") {
            textIndent.hanging = true;
        } else if (param == "each-line") {
            textIndent.eachLine = true;
        } else {
            textIndent.length = SvgUtil::parseTextUnitStruct(context.currentGC(), toPkString(param));
            //ToDo: figure out how to detect value is number.
            //qWarning() << "Unknown parameter in text-indent" << param;
        }
    }
    return textIndent;
}

PkString writeTextIndent(const TextIndentInfo textIndent)
{
    PkStringList values;
    values.append(writeLengthPercentage(textIndent.length));
    if (textIndent.hanging) {
        values.append("hanging");
    }
    if (textIndent.eachLine) {
        values.append("each-line");
    }
    return values.join(" ");
}

TabSizeInfo parseTabSize(const PkString &value, const SvgLoadingContext &context)
{
    TabSizeInfo tabSizeInfo;
    qreal val = KisDomUtils::toDouble(toPkString(value), &tabSizeInfo.isNumber);
    if (tabSizeInfo.isNumber) {
        tabSizeInfo.value = qMax(0.0, val);
    } else {
        tabSizeInfo.length = SvgUtil::parseTextUnitStruct(context.currentGC(), toPkString(value));
    }
    if ((tabSizeInfo.isNumber && tabSizeInfo.value < 0) || tabSizeInfo.length.value < 0) {
        tabSizeInfo.isNumber = true;
        tabSizeInfo.value = 0;
        tabSizeInfo.length.value = 0;
    }
    return tabSizeInfo;
}

PkString writeTabSize(const TabSizeInfo tabSize)
{
    PkString val = KisDomUtils::toString(tabSize.value);
    if (!tabSize.isNumber) {

        // Tabsize does not support percentage, so if we accidentally set it somewhere, convert to em.
        val = writeLengthPercentage(tabSize.length, true);
        if (tabSize.length == CssLengthPercentage::Absolute) {
            val += "px"; // In SVG, due to browsers, the default unit is css px. Krita scales these to pt.
        }
    }
    return val;
}

int parseCSSFontStretch(const PkString &value, int currentStretch)
{
    int newStretch = 100;

    static constexpr std::array<int, 9> fontStretches = {50, 62, 75, 87, 100, 112, 125, 150, 200};

    if (value == "wider") {
        const auto it = std::upper_bound(fontStretches.begin(), fontStretches.end(), currentStretch);

        newStretch = it != fontStretches.end() ? *it : fontStretches.back();
    } else if (value == "narrower") {
        const auto it =
            std::upper_bound(fontStretches.rbegin(), fontStretches.rend(), currentStretch, std::greater<int>());

        newStretch = it != fontStretches.rend() ? *it : fontStretches.front();
    } else {
        // try to read numerical stretch value
        bool ok = false;
        newStretch = value.toInt(&ok, 10);

        if (!ok) {
            auto it = std::find(fontStretchNames.begin(), fontStretchNames.end(), value);
            if (it != fontStretchNames.end()) {
                const auto index = std::distance(fontStretchNames.begin(), it);
                KIS_ASSERT(index >= 0);
                newStretch = fontStretches.at(static_cast<size_t>(index));
            }
        }
    }
    return newStretch;
}

int parseCSSFontWeight(const PkString &value, int currentWeight)
{
    int weight = 400;

    // map svg weight to qt weight
    // svg value        qt value
    // 100,200,300      1, 17, 33
    // 400              50          (normal)
    // 500,600          58,66
    // 700              75          (bold)
    // 800,900          87,99
    static constexpr std::array<int, 9> svgFontWeights = {100, 200, 300, 400, 500, 600, 700, 800, 900};

    if (value == "bold")
        weight = 700;
    else if (value == "bolder") {
        const auto it = std::upper_bound(svgFontWeights.begin(), svgFontWeights.end(), currentWeight);

        weight = it != svgFontWeights.end() ? *it : svgFontWeights.back();
    } else if (value == "lighter") {
        const auto it =
            std::upper_bound(svgFontWeights.rbegin(), svgFontWeights.rend(), currentWeight, std::greater<int>());

        weight = it != svgFontWeights.rend() ? *it : svgFontWeights.front();
    } else {
        bool ok = false;

        // try to read numerical weight value
        const int parsed = value.toInt(&ok, 10);
        if (ok) {
            weight = qBound(0, parsed, 1000);
        }
    }
    return weight;
}

LineHeightInfo parseLineHeight(const PkString &value, const SvgLoadingContext &context)
{
    LineHeightInfo lineHeight;
    lineHeight.isNormal = value == "normal";
    qreal parsed = value.toDouble(&lineHeight.isNumber);

    if (lineHeight.isNumber) {
        lineHeight.value = parsed;
    } else {
        lineHeight.length = SvgUtil::parseTextUnitStruct(context.currentGC(), toPkString(value));
    }

    // Negative line-height is invalid
    if (!lineHeight.isNormal && (lineHeight.value < 0 || lineHeight.length.value < 0)) {
        lineHeight.isNormal = true;
        lineHeight.isNumber = false;
        lineHeight.value = 0;
        lineHeight.length.value = 0;
    }

    return lineHeight;
}

PkString writeLineHeight(LineHeightInfo lineHeight)
{
    if (lineHeight.isNormal) return PkString("normal");
    PkString val = KisDomUtils::toString(lineHeight.value);
    if (!lineHeight.isNumber) {
        val = writeLengthPercentage(lineHeight.length);
        if (lineHeight.length.unit == CssLengthPercentage::Absolute) {
            val += "px"; // In SVG, due to browsers, the default unit is css px. Krita scales these to pt.
        }
    }
    return val;
}

QDebug operator<<(QDebug dbg, const LineHeightInfo &value)
{
    dbg.nospace() << "LineHeightInfo(";

    if (value.isNormal) {
        dbg.nospace() << "normal";
    } else if (!value.isNumber) {
        dbg.nospace() << value.value << "pt";
    } else {
        dbg.nospace() << value.value;
    }

    dbg.nospace() << ")";
    return dbg.space();
}

QDebug operator<<(QDebug dbg, const CssLengthPercentage &value)
{
    dbg.nospace() << "Length(";

    if (value.unit == CssLengthPercentage::Percentage) {
        dbg.nospace() << value.value << "%";
    } else if (value.unit == CssLengthPercentage::Em) {
        dbg.nospace() << value.value << "em";
    } else if (value.unit == CssLengthPercentage::Ex) {
        dbg.nospace() << value.value << "ex";
    } else if (value.unit == CssLengthPercentage::Cap) {
        dbg.nospace() << value.value << "cap";
    } else if (value.unit == CssLengthPercentage::Ch) {
        dbg.nospace() << value.value << "ch";
    } else if (value.unit == CssLengthPercentage::Ic) {
        dbg.nospace() << value.value << "ic";
    } else if (value.unit == CssLengthPercentage::Lh) {
        dbg.nospace() << value.value << "lh";
    } else {
        dbg.nospace() << value.value << "(pt)";
    }

    dbg.nospace() << ")";
    return dbg.space();
}

PkString writeLengthPercentage(const CssLengthPercentage &length, bool percentageAsEm)
{
    PkString val;
    if (length.unit == CssLengthPercentage::Percentage && !percentageAsEm) {
        val = KisDomUtils::toString(length.value*100.0) + "%";
    } else {
        val = KisDomUtils::toString(length.value);
        if (length.unit == CssLengthPercentage::Em || length.unit == CssLengthPercentage::Percentage) {
            val += "em";
        } else if (length.unit == CssLengthPercentage::Ex) {
            val += "ex";
        } else if (length.unit == CssLengthPercentage::Cap) {
            val += "cap";
        } else if (length.unit == CssLengthPercentage::Ch) {
            val += "ch";
        } else if (length.unit == CssLengthPercentage::Ic) {
            val += "ic";
        } else if (length.unit == CssLengthPercentage::Lh) {
            val += "lh";
        }
    }
    return val;
}

void CssLengthPercentage::convertToAbsolute(const KoSvgText::FontMetrics metrics, const qreal fontSize, const CssLengthPercentage::UnitType percentageUnit) {
    UnitType u = unit;
    if (u == Percentage) {
        u = percentageUnit;
    }

    const qreal ftMultiplier = fontSize / metrics.fontSize;
    if (u == Em) {
        value = value * fontSize;
    } else if (u == Ex) {
        value = value * metrics.xHeight * ftMultiplier;
    } else if (u == Cap) {
        value = value * metrics.capHeight * ftMultiplier;
    } else if (u == Ch) {
        value = value * metrics.zeroAdvance * ftMultiplier;
    } else if (u == Ic) {
        value = value * metrics.ideographicAdvance * ftMultiplier;
    } else if (u == Lh) {
        value = value * (metrics.ascender - metrics.descender + metrics.lineGap) * ftMultiplier;
    }
    unit = Absolute;
}

AutoLengthPercentage parseAutoLengthPercentageXY(const PkString &value, const SvgLoadingContext &context, const PkString &autoKeyword, PkRectF bbox, bool percentageIsViewPort)
{
    return value == autoKeyword ? AutoLengthPercentage()
                                : percentageIsViewPort? AutoLengthPercentage(SvgUtil::parseUnitStruct(context.currentGC(), toPkString(value), true, true, toPkRectF(bbox)))
                                                      : AutoLengthPercentage(SvgUtil::parseTextUnitStruct(context.currentGC(), toPkString(value)));
}

PkString writeAutoLengthPercentage(const AutoLengthPercentage &value, const PkString &autoKeyword, bool percentageToEm)
{
    return value.isAuto ? autoKeyword : writeLengthPercentage(value.length, percentageToEm);
}

QDebug operator<<(QDebug dbg, const KoSvgText::AutoLengthPercentage &value)
{
    if (value.isAuto) {
        dbg.nospace() << "auto";
    } else {
        dbg.nospace() << value.length;
    }
    return dbg.space();
}


QDebug operator<<(QDebug dbg, const KoSvgText::FontFamilyAxis &axis)
{
    dbg.nospace() << axis.debugInfo();
    return dbg.space();
}

PkDataStream &operator<<(PkDataStream &out, const KoSvgText::FontFamilyAxis &axis) {

    PkXmlDocument doc;
    PkXmlElement root = doc.createElement("axis");
    root.setAttribute("tagName", axis.tag);
    root.setAttribute("min", PkString::number(axis.min));
    root.setAttribute("max", PkString::number(axis.max));
    root.setAttribute("default", PkString::number(axis.defaultValue));
    root.setAttribute("hidden", axis.axisHidden? "true": "false");
    root.setAttribute("variable", axis.variableAxis? "true": "false");
    for(auto it = axis.localizedLabels.begin(); it != axis.localizedLabels.end(); it++) {
        PkXmlElement name = doc.createElement("name");
        name.setAttribute("lang", toPkString(it.key().bcp47Name()));
        name.setAttribute("value", it.value());
        root.appendChild(name);
    }
    doc.appendChild(root);
    out << doc.toString(0);
    return out;
}
PkDataStream &operator>>(PkDataStream &in, KoSvgText::FontFamilyAxis &axis) {

    PkString xml;
    in >> xml;

    PkXmlDocument doc;
    doc.setContent(xml);
    PkXmlElement root = doc.childNodes().at(0).toElement();
    axis.tag = root.attribute("tagName");
    axis.min = root.attribute("min").toDouble();
    axis.max = root.attribute("max").toDouble();
    axis.defaultValue = root.attribute("default").toDouble();
    axis.axisHidden = root.attribute("hidden") == "true"? true: false;
    axis.variableAxis = root.attribute("variable") == "true"? true: false;
    PkXmlNodeList names =  root.elementsByTagName("name");
    for(int i = 0; i < names.size(); i++) {
        PkXmlElement name = names.at(i).toElement();
        PkString lang = name.attribute("lang");
        PkString value = name.attribute("value");
        axis.localizedLabels.insert(QLocale(toQString(lang)), value);
    }

    return in;
}

QDebug operator<<(QDebug dbg, const KoSvgText::FontFamilyStyleInfo &style)
{
    dbg.nospace() << style.debugInfo();
    return dbg.space();
}

PkDataStream &operator<<(PkDataStream &out, const KoSvgText::FontFamilyStyleInfo &style) {

    PkXmlDocument doc;
    PkXmlElement root = doc.createElement("style");
    root.setAttribute("italic", style.isItalic? "true": "false");
    root.setAttribute("oblique", style.isOblique? "true": "false");
    for(auto it = style.instanceCoords.begin(); it != style.instanceCoords.end(); it++) {
        PkXmlElement coord = doc.createElement("coord");
        coord.setAttribute("tag", it.key());
        coord.setAttribute("value", PkString::number(it.value()));
        root.appendChild(coord);
    }
    for(auto it = style.localizedLabels.begin(); it != style.localizedLabels.end(); it++) {
        PkXmlElement name = doc.createElement("name");
        name.setAttribute("lang", toPkString(it.key().bcp47Name()));
        name.setAttribute("value", it.value());
        root.appendChild(name);
    }
    doc.appendChild(root);
    out << doc.toString(0);
    return out;
}
PkDataStream &operator>>(PkDataStream &in, KoSvgText::FontFamilyStyleInfo &style) {
    PkString xml;
    in >> xml;

    PkXmlDocument doc;
    doc.setContent(xml);
    PkXmlElement root = doc.childNodes().at(0).toElement();
    style.isItalic = root.attribute("italic") == "true"? true: false;
    style.isOblique = root.attribute("oblique") == "true"? true: false;
    PkXmlNodeList names =  root.elementsByTagName("name");
    for(int i = 0; i < names.size(); i++) {
        PkXmlElement name = names.at(i).toElement();
        PkString lang = name.attribute("lang");
        PkString value = name.attribute("value");
        style.localizedLabels.insert(QLocale(toQString(lang)), value);
    }
    PkXmlNodeList coords =  root.elementsByTagName("coord");
    for(int i = 0; i < coords.size(); i++) {
        PkXmlElement coord = coords.at(i).toElement();
        PkString tag = coord.attribute("tag");
        double value = coord.attribute("value").toDouble();
        style.instanceCoords.insert(tag, value);
    }

    return in;
}

CssFontStyleData parseFontStyle(const PkString &value)
{
    CssFontStyleData slant;
    PkStringList params = value.split(" ");
    if (!params.isEmpty()) {
        PkString style = params.first();
        slant.style = style == "italic"? QFont::StyleItalic: style == "oblique"? QFont::StyleOblique: QFont::StyleNormal;
    }
    if (params.size() > 1) {
        PkString angle = params.last();
        if (angle.endsWith("deg")) {
            angle.chop(3);
            slant.slantValue.isAuto = false;
            slant.slantValue.customValue = angle.toDouble();
        }
    }
    return slant;
}

PkString writeFontStyle(CssFontStyleData value)
{
    PkString style =
        value.style == QFont::StyleItalic ? "italic" :
        value.style == QFont::StyleOblique ? "oblique" : "normal";
    if (value.style == QFont::StyleOblique && !value.slantValue.isAuto) {
        style.append(PkString(" ")+PkString::number(value.slantValue.customValue)+PkString("deg"));
    }
    return style;
}

FontFeatureLigatures parseFontFeatureLigatures(const PkString &value, FontFeatureLigatures features)
{
    if (value == "common-ligatures") {
        features.commonLigatures = true;
    } else if (value == "no-common-ligatures") {
        features.commonLigatures = false;
    } else if (value == "discretionary-ligatures") {
        features.discretionaryLigatures = true;
    } else if (value == "no-discretionary-ligatures") {
        features.discretionaryLigatures = false;
    } else if (value == "historical-ligatures") {
        features.historicalLigatures = true;
    } else if (value == "no-historical-ligatures") {
        features.historicalLigatures = false;
    } else if (value == "contextual") {
        features.contextualAlternates = true;
    } else if (value == "no-contextual") {
        features.contextualAlternates = false;
    } else if (value == "none") {
        features.commonLigatures = false;
        features.discretionaryLigatures = false;
        features.historicalLigatures = false;
        features.contextualAlternates = false;
    }
    return features;
}

PkString writeFontFeatureLigatures(const FontFeatureLigatures &feature)
{
    if (feature.commonLigatures && !feature.discretionaryLigatures
            && !feature.historicalLigatures && feature.commonLigatures) {
        return "normal";
    }
    if (!feature.commonLigatures && !feature.discretionaryLigatures
            && !feature.historicalLigatures && !feature.commonLigatures) {
        return "none";
    }
    PkStringList list;
    if (!feature.commonLigatures) {
        list << "no-common-ligatures";
    }
    if (feature.discretionaryLigatures) {
        list << "discretionary-ligatures";
    }
    if (feature.historicalLigatures) {
        list << "historical-ligatures";
    }
    if (!feature.contextualAlternates) {
        list << "no-contextual";
    }
    return list.join(" ");
}

QDebug operator<<(QDebug dbg, const KoSvgText::FontFeatureLigatures &feature)
{
    dbg.nospace() << "Ligatures("<< writeFontFeatureLigatures(feature) <<")";
    return dbg.space();
}

FontFeatureNumeric parseFontFeatureNumeric(const PkString &value, FontFeatureNumeric features)
{
    if (value == "lining-nums") {
        features.style = NumericFigureStyleLining;
    } else if (value == "oldstyle-nums") {
        features.style = NumericFigureStyleOld;
    } else if (value == "proportional-nums") {
        features.spacing = NumericFigureSpacingProportional;
    } else if (value == "tabular-nums") {
        features.spacing = NumericFigureSpacingTabular;
    } else if (value == "diagonal-fractions") {
        features.fractions = NumericFractionsDiagonal;
    } else if (value == "stacked-fractions") {
        features.fractions = NumericFractionsStacked;
    } else if (value == "ordinal") {
        features.ordinals = true;
    } else if (value == "slashed-zero") {
        features.slashedZero = true;
    } else {
        features = FontFeatureNumeric();
    }
    return features;
}

PkString writeFontFeatureNumeric(const FontFeatureNumeric &feature)
{
    if (feature == FontFeatureNumeric()) {
        return "normal";
    }
    PkStringList list;

    if (feature.style == NumericFigureStyleLining) {
        list << "lining-nums";
    } else if (feature.style == NumericFigureStyleOld) {
        list << "oldstyle-nums";
    }

    if (feature.spacing == NumericFigureSpacingProportional) {
        list << "proportional-nums";
    } else if (feature.spacing == NumericFigureSpacingTabular) {
        list << "tabular-nums";
    }

    if (feature.fractions == NumericFractionsDiagonal) {
        list << "diagonal-fractions";
    } else if (feature.fractions == NumericFractionsStacked) {
        list << "stacked-fractions";
    }

    if (feature.ordinals) {
        list << "ordinal";
    }

    if (feature.slashedZero) {
        list << "slashed-zero";
    }

    return list.join(" ");
}

QDebug operator<<(QDebug dbg, const KoSvgText::FontFeatureNumeric &feature)
{
    dbg.nospace() << "NumericFeatures("<< writeFontFeatureNumeric(feature) <<")";
    return dbg.space();
}

FontFeatureEastAsian parseFontFeatureEastAsian(const PkString &value, FontFeatureEastAsian features)
{
    if (value == "jis78") {
        features.variant = EastAsianJis78;
    } else if (value == "jis83") {
        features.variant = EastAsianJis83;
    } else if (value == "jis90") {
        features.variant = EastAsianJis90;
    } else if (value == "jis04") {
        features.variant = EastAsianJis04;
    } else if (value == "simplified") {
        features.variant = EastAsianSimplified;
    } else if (value == "traditional") {
        features.variant = EastAsianTraditional;
    } else if (value == "full-width") {
        features.width = EastAsianFullWidth;
    } else if (value == "proportional-width") {
        features.width = EastAsianProportionalWidth;
    } else if (value == "ruby") {
        features.ruby = true;
    } else {
        features = FontFeatureEastAsian();
    }
    return features;
}

PkString writeFontFeatureEastAsian(const FontFeatureEastAsian &feature)
{
    if (feature == FontFeatureEastAsian()) {
        return "normal";
    }
    PkStringList list;

    if (feature.variant == EastAsianJis78) {
        list << "jis78";
    } else if (feature.variant == EastAsianJis83) {
        list << "jis83";
    } else if (feature.variant == EastAsianJis90) {
        list << "jis90";
    } else if (feature.variant == EastAsianJis04) {
        list << "jis04";
    } else if (feature.variant == EastAsianSimplified) {
        list << "simplified";
    } else if (feature.variant == EastAsianTraditional) {
        list << "traditional";
    }

    if (feature.width == EastAsianFullWidth) {
        list << "full-width";
    } else if (feature.width == EastAsianProportionalWidth) {
        list << "proportional-width";
    }

    if (feature.ruby) {
        list << "ruby";
    }

    return list.join(" ");
}

QDebug operator<<(QDebug dbg, const KoSvgText::FontFeatureEastAsian &feature)
{
    dbg.nospace() << "EastAsianFeatures("<< writeFontFeatureEastAsian(feature) <<")";
    return dbg.space();
}

FontFeaturePosition parseFontFeaturePosition(const PkString &value, FontFeaturePosition feature)
{
    return value == "super"? PositionSuper : value == "sub"? PositionSub : value == "normal"? PositionNormal: feature;
}

PkString writeFontFeaturePosition(const FontFeaturePosition &value)
{
    return value == PositionSuper ? "super"
                                  : value == PositionSub? "sub" : "normal";
}

FontFeatureCaps parseFontFeatureCaps(const PkString &value, FontFeatureCaps feature)
{
    return value == "small-caps"          ? CapsSmall
        : value == "all-small-caps"        ? CapsAllSmall
        : value == "petite-caps"         ? CapsPetite
        : value == "all-petite-caps" ? CapsAllPetite
        : value == "unicase"       ? CapsUnicase
        : value == "titling-caps"       ? CapsTitling
        : value == "normal"   ? CapsNormal: feature;
}

PkString writeFontFeatureCaps(const FontFeatureCaps &value)
{
    return value == CapsSmall          ? "small-caps"
        : value == CapsAllSmall        ? "all-small-caps"
        : value == CapsPetite         ? "petite-caps"
        : value == CapsAllPetite ? "all-petite-caps"
        : value == CapsUnicase       ? "unicase"
        : value == CapsTitling       ? "titling-caps"
                                     : "normal";
}

PkStringList fontFeaturesPosition(const FontFeaturePosition &feature, const int start, const int end)
{
    const PkString length = PkString("[%1:%2]").arg(start).arg(end);
    PkString tag = feature == PositionSuper? "sups" : feature == PositionSub? "subs": PkString();
    if (!tag.isEmpty()) {
        tag += length;
        tag += "=1";
    }
    return tag.isEmpty()? PkStringList(): PkStringList{tag};
}

PkStringList fontFeaturesCaps(const FontFeatureCaps &feature, const int start, const int end)
{
    PkStringList list;
    const PkString length = PkString("[%1:%2]").arg(start).arg(end);

    switch (feature) {
    case CapsSmall:
        list << "smcp" + length + "=1";
        break;
    case CapsAllSmall:
        list << "smcp" + length + "=1";
        list << "c2sc" + length + "=1";
        break;
    case CapsPetite:
        list << "pcap" + length + "=1";
        break;
    case CapsAllPetite:
        list << "pcap" + length + "=1";
        list << "c2pc" + length + "=1";
        break;
    case CapsUnicase:
        list << "unic" + length + "=1";
        break;
    case CapsTitling:
        list << "titl" + length + "=1";
        break;
    default:
        break;
    }

    return list;
}

FontMetrics::FontMetrics(qreal fontSizeInPt, bool isHorizontal)
    : isVertical(!isHorizontal)
    , fontSize(fontSizeInPt * 64.0)
{
    ideographicAdvance = fontSize;
    xHeight = fontSize/2;
    capHeight = (fontSize / 5) * 4;

    subScriptOffset.second = -(fontSize / 5);
    superScriptOffset.second = (fontSize / 3);
    if (isHorizontal) {
        zeroAdvance = fontSize/2;
        spaceAdvance = fontSize/2;

        ascender = (fontSize / 5) * 4;
        descender = ascender - fontSize;

        mathematicalBaseline = xHeight/2;

        ideographicUnderBaseline = descender;
        ideographicOverBaseline = ascender;
        ideographicCenterBaseline = (ascender+descender)/2;

        hangingBaseline = (fontSize / 5) * 3;

        caretRun = 0;
        caretRise = 1;
        caretOffset = 0;

    } else {
        zeroAdvance = fontSize;
        spaceAdvance = fontSize;

        ascender = fontSize /2;
        descender = ascender - fontSize;

        ideographicUnderBaseline = descender;
        ideographicOverBaseline = ascender;
        ideographicCenterBaseline = (ascender+descender)/2;

        mathematicalBaseline = ideographicCenterBaseline;

        alphabeticBaseline = ascender - (fontSize / 5) * 4;

        hangingBaseline = alphabeticBaseline + ((fontSize / 5) * 3);

        caretRun = 1;
        caretRise = 0;
        caretOffset = 0;
    }

    ideographicFaceUnderBaseline = descender;
    ideographicFaceOverBaseline = ascender;
    lineThroughOffset = mathematicalBaseline;
}

bool FontMetrics::operator==(const FontMetrics &other) const {
    return isVertical == other.isVertical
            && fontSize == other.fontSize
            && zeroAdvance == other.zeroAdvance
            && spaceAdvance == other.spaceAdvance
            && ideographicAdvance == other.ideographicAdvance
            && xHeight == other.xHeight
            && capHeight == other.capHeight
            && subScriptOffset == other.subScriptOffset
            && superScriptOffset == other.superScriptOffset
            && ascender == other.ascender
            && descender == other.descender
            && lineGap == other.lineGap
            && alphabeticBaseline == other.alphabeticBaseline
            && mathematicalBaseline == other.mathematicalBaseline
            && ideographicUnderBaseline == other.ideographicUnderBaseline
            && ideographicCenterBaseline == other.ideographicCenterBaseline
            && ideographicOverBaseline == other.ideographicOverBaseline
            && ideographicFaceUnderBaseline == other.ideographicFaceUnderBaseline
            && ideographicFaceOverBaseline == other.ideographicFaceOverBaseline
            && hangingBaseline == other.hangingBaseline
            && lineThroughOffset == other.lineThroughOffset
            && lineThroughThickness == other.lineThroughThickness
            && underlineOffset == other.underlineOffset
            && underlineThickness == other.underlineThickness
            && caretRun == other.caretRun
            && caretRise == other.caretRise
            && caretOffset == other.caretOffset;
}

int FontMetrics::valueForBaselineValue(Baseline baseline) const {
    qint32 baselineVal = 0;
    switch(baseline) {
    case BaselineIdeographic:
        baselineVal =  ideographicUnderBaseline;
        break;
    case BaselineAlphabetic:
        baselineVal = alphabeticBaseline;
        break;
    case BaselineHanging:
        baselineVal = hangingBaseline;
        break;
    case BaselineMathematical:
        baselineVal = mathematicalBaseline;
        break;
    case BaselineCentral:
        baselineVal = ideographicCenterBaseline;
        break;
    case BaselineMiddle:
        baselineVal = isVertical? ideographicCenterBaseline: xHeight/2;
        break;
    case BaselineTextBottom:
        baselineVal = descender;
        break;
    case BaselineTextTop:
        baselineVal = ascender;
        break;
    default:
        break;
    }
    return baselineVal;
}

void FontMetrics::setBaselineValueByTag(const PkString &tag, int32_t value) {
    if (tag == "romn") {
        alphabeticBaseline = value;
    } else if (tag == "hang") {
        hangingBaseline = value;
    } else if (tag == "icfb") {
        ideographicFaceUnderBaseline = value;
    } else if (tag == "icft") {
        ideographicFaceOverBaseline = value;
    } else if (tag == "ideo") {
        ideographicUnderBaseline = value;
    } else if (tag == "idtp") {
        ideographicOverBaseline = value;
    } else if (tag == "Idce") {
        ideographicCenterBaseline = value;
    } else if (tag == "math") {
        mathematicalBaseline = value;
    }
}

void FontMetrics::setMetricsValueByTag(const QLatin1String &tag, int32_t value) {
    if (tag == "xhgt") {
        xHeight = value;
    } else if (tag == "cpht") {
        capHeight = value;
    } else if (tag == "sbxo") {
        subScriptOffset.first = value;
    } else if (tag == "sbyo") {
        subScriptOffset.second = value;
    } else if (tag == "spxo") {
        superScriptOffset.first = value;
    } else if (tag == "spyo") {
        superScriptOffset.second = value;
    } else if (tag == "strs") {
        lineThroughThickness = value;
    } else if (tag == "stro") {
        lineThroughOffset = value;
    } else if (tag == "unds") {
        underlineThickness = value;
    } else if (tag == "undo") {
        underlineOffset = value;
    } else if (tag == "hcrs") {
        caretRise = value;
    } else if (tag == "hcrn") {
        caretRun = value;
    } else if (tag == "hcof") {
        caretOffset = value;
    } else if (tag == "vcrs") {
        caretRise = value;
    } else if (tag == "vcrn") {
        caretRun = value;
    } else if (tag == "vcof") {
        caretOffset = value;
    }
}

void FontMetrics::scaleBaselines(const qreal multiplier)
{
    alphabeticBaseline *= multiplier;
    hangingBaseline *= multiplier;
    mathematicalBaseline *= multiplier;
    ideographicFaceUnderBaseline *= multiplier;
    ideographicFaceOverBaseline *= multiplier;
    ideographicOverBaseline *= multiplier;
    ideographicCenterBaseline *= multiplier;
    ideographicUnderBaseline *= multiplier;
    xHeight *= multiplier;
    capHeight *= multiplier;
    subScriptOffset.first *= multiplier;
    subScriptOffset.second *= multiplier;
    superScriptOffset.first *= multiplier;
    superScriptOffset.second *= multiplier;
    fontSize *= multiplier;
    ascender *= multiplier;
    descender *= multiplier;
    lineGap *= multiplier;

    zeroAdvance *= multiplier;
    spaceAdvance *= multiplier;
    ideographicAdvance *= multiplier;

    underlineOffset *= multiplier;
    underlineThickness *= multiplier;
    lineThroughOffset *= multiplier;
    lineThroughThickness *= multiplier;
}

void FontMetrics::offsetMetricsToNewOrigin(const Baseline baseline)
{
    qint32 offset = valueForBaselineValue(baseline);
    if (offset == 0) return;

    alphabeticBaseline -= offset;
    hangingBaseline -= offset;
    mathematicalBaseline -= offset;
    ideographicFaceUnderBaseline -= offset;
    ideographicFaceOverBaseline -= offset;
    ideographicOverBaseline -= offset;
    ideographicCenterBaseline -= offset;
    ideographicUnderBaseline -= offset;

    ascender -= offset;
    descender -= offset;
}

QDebug operator<<(QDebug dbg, const FontMetrics &metrics)
{
    const double ftPixel = 1.0;
    dbg.nospace() << "FontMetrics(";
    dbg.nospace() << "Direction: " << (metrics.isVertical? "Top to bottom. ": "Left to right. ");
    dbg.nospace() << "FontSize: " << PkString::number(metrics.fontSize*ftPixel) << "px. ";
    dbg.nospace() << "Number width: " << PkString::number(metrics.zeroAdvance*ftPixel) << "px. ";
    dbg.nospace() << "Space width: " << PkString::number(metrics.spaceAdvance*ftPixel) << "px. ";
    dbg.nospace() << "Ideographic width: " << PkString::number(metrics.ideographicAdvance*ftPixel) << "px. ";

    dbg.nospace() << "xHeight: " << PkString::number(metrics.xHeight*ftPixel) << "px. ";
    dbg.nospace() << "cap height: " << PkString::number(metrics.capHeight*ftPixel) << "px. ";
    dbg.nospace() << "Subscripts: " << PkString::number(metrics.subScriptOffset.second*ftPixel) << "px. ";
    dbg.nospace() << "Superscripts: " << PkString::number(metrics.superScriptOffset.second*ftPixel) << "px. ";
    dbg.nospace() << "Ascender: " << PkString::number(metrics.ascender*ftPixel) << "px. ";
    dbg.nospace() << "Descender: " << PkString::number(metrics.descender*ftPixel) << "px. ";
    dbg.nospace() << "Linegap: " << PkString::number(metrics.lineGap*ftPixel) << "px. ";

    dbg.nospace() << "Alphabetic: " << PkString::number(metrics.alphabeticBaseline*ftPixel) << "px. ";
    dbg.nospace() << "Middle: " << PkString::number((metrics.xHeight/2)*ftPixel) << "px. ";
    dbg.nospace() << "Mathematical: " << PkString::number(metrics.mathematicalBaseline*ftPixel) << "px. ";

    dbg.nospace() << "Ideo Over: " << PkString::number(metrics.ideographicOverBaseline*ftPixel) << "px. ";
    dbg.nospace() << "Central: " << PkString::number(metrics.ideographicCenterBaseline*ftPixel) << "px. ";
    dbg.nospace() << "Ideo Under: " << PkString::number(metrics.ideographicUnderBaseline*ftPixel) << "px. ";

    dbg.nospace() << "Ideo Face Over: " << PkString::number(metrics.ideographicFaceOverBaseline*ftPixel) << "px. ";
    dbg.nospace() << "Ideo Face Under: " << PkString::number(metrics.ideographicFaceUnderBaseline*ftPixel) << "px. ";
    dbg.nospace() << "Hanging: " << PkString::number(metrics.hangingBaseline*ftPixel) << "px. ";
    dbg.nospace() << ")";
    return dbg.space();
}

QDebug operator<<(QDebug dbg, const KoSvgText::TextUnderlinePosition &value)
{
    dbg.nospace() << "Underline position( horizontal:" << value.horizontalPosition << ", vertical:" << value.verticalPosition << ")";
    return dbg.space();
}

TextRendering parseTextRendering(const PkString &value)
{
    if (value == "optimizeSpeed") {
        return RenderingOptimizeSpeed;
    } else if (value == "optimizeLegibility") {
        return RenderingOptimizeLegibility;
    } else if (value == "geometricPrecision") {
        return RenderingGeometricPrecision;
    }
    return RenderingAuto;
}

PkString writeTextRendering(TextRendering value)
{
    if (value == RenderingOptimizeSpeed) {
        return "optimizeSpeed";
    } else if (value == RenderingOptimizeLegibility) {
        return "optimizeLegibility";
    } else if (value == RenderingGeometricPrecision) {
        return "geometricPrecision";
    } else {
        return "auto";
    }
}

qreal ResolutionHandler::freeTypePixelToPointFactor(const bool x) const {
    return (1.0/freeTypePixel) * pixelToPointFactor(x);
}

PkTransform ResolutionHandler::freeTypeToPixelTransform() const {
    return PkTransform::fromScale(1/freeTypePixel, -1/freeTypePixel);
}

PkTransform ResolutionHandler::freeTypeToPointTransform() const
{
    return freeTypeToPixelTransform()*pixelToPoint();
}

PkTransform ResolutionHandler::pixelToPoint() const {
    return PkTransform::fromScale(pointInInch / xRes, pointInInch / yRes);
}

PkPointF ResolutionHandler::adjust(const PkPointF point) const {
    if (!roundToPixelHorizontal && !roundToPixelVertical) return point;
    PkPointF pix = pointToPixel().map(point);
    if (roundToPixelHorizontal) {
        pix.setX(qRound(pix.x()));
    }
    if (roundToPixelVertical) {
        pix.setY(qRound(pix.y()));
    }
    return pixelToPoint().map(pix);
}

PkPointF ResolutionHandler::adjustFloor(const PkPointF point) const
{
    if (!roundToPixelHorizontal && !roundToPixelVertical) return point;
    PkPointF pix = pointToPixel().map(point);
    if (roundToPixelHorizontal) {
        pix.setX(floor(pix.x()));
    }
    if (roundToPixelVertical) {
        pix.setY(floor(pix.y()));
    }
    return pixelToPoint().map(pix);
}

PkPointF ResolutionHandler::adjustCeil(const PkPointF point) const
{
    if (!roundToPixelHorizontal && !roundToPixelVertical) return point;
    PkPointF pix = pointToPixel().map(point);
    if (roundToPixelHorizontal) {
        pix.setX(ceil(pix.x()));
    }
    if (roundToPixelVertical) {
        pix.setY(ceil(pix.y()));
    }
    return pixelToPoint().map(pix);
}

PkPointF ResolutionHandler::adjustWithOffset(const PkPointF point, const PkPointF offset) const
{
    if (!roundToPixelHorizontal && !roundToPixelVertical) return point;
    return adjust(point+offset)-offset;
}

PkRectF ResolutionHandler::adjust(const PkRectF rect) const {
    return PkRectF(adjust(rect.topLeft()), adjust(rect.bottomRight()));
}

qreal ResolutionHandler::pointToPixelFactor(const bool x) const {
    return x? xRes/pointInInch: yRes/pointInInch;
}

PkTransform ResolutionHandler::pointToPixel() const {
    return PkTransform::fromScale(xRes/pointInInch, yRes/pointInInch);
}

qreal ResolutionHandler::pixelToPointFactor(const bool x) const {
    return x? pointInInch/xRes: pointInInch/yRes;
}

} // namespace KoSvgText
