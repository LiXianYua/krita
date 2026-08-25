/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2009 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QtCore/QtCore>
#include <PkFlakeBridge.h>

#include "SvgUtil.h"
#include "SvgGraphicContext.h"

#include <KoUnit.h>
#include <KoSvgText.h>

#include <pk/string/PkString.h>

#include <math.h>
#include <regex>
#include <string>
#include "kis_debug.h"
#include "kis_global.h"

#include <KoXmlWriter.h>
#include "kis_dom_utils.h"

#define DPI 72.0

#define DEG2RAD(degree) degree/180.0*M_PI

namespace {

// 辅助：PkString 没有 endsWith（对齐旧 Q 系字符串的 endsWith 语义）。
bool pkEndsWith(const PkString &s, const PkString &suffix)
{
    if (suffix.size() > s.size()) {
        return false;
    }
    return s.mid(s.size() - suffix.size()) == suffix;
}

// PkString 没有 indexOf(char, from)（对齐 Qt5 的 indexOf）。
int pkIndexOf(const PkString &s, char16_t ch, int from = 0)
{
    for (int i = from; i < s.size(); ++i) {
        if (s.at(i) == ch) {
            return i;
        }
    }
    return -1;
}

// 按分隔符切分并丢弃空段（对齐 Qt5 的 split(sep, Qt::SkipEmptyParts)）。
PkStringList pkSplitSkipEmpty(const PkString &s, char16_t sep)
{
    PkStringList result;
    for (const PkString &part : s.split(sep)) {
        if (!part.isEmpty()) {
            result.push_back(part);
        }
    }
    return result;
}

// 空白折叠为单空格并 trim（对齐 Qt5 的 simplified）。
PkString pkSimplified(const PkString &s)
{
    const std::string in = s.PkToUtf8();
    std::string out;
    out.reserve(in.size());
    bool lastWasSpace = false;
    bool leading = true;
    for (const char c : in) {
        const bool isWs = (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v');
        if (isWs) {
            if (lastWasSpace || leading) {
                continue;
            }
            lastWasSpace = true;
            out.push_back(' ');
        } else {
            lastWasSpace = false;
            leading = false;
            out.push_back(c);
        }
    }
    return PkString::PkFromUtf8(out.data(), static_cast<int>(out.size()));
}

// 把全部 from 替换为 to（对齐 Qt5 的 replace(ch1, ch2)）。
PkString pkReplaceChar(const PkString &s, char16_t from, char16_t to)
{
    PkString out;
    for (int i = 0; i < s.size(); ++i) {
        char16_t c = s.at(i);
        if (c == from) {
            c = to;
        }
        out += pkCharToString(c);
    }
    return out;
}

// 移除全部指定字符（对齐 Qt5 的 remove(ch)）。
PkString pkRemoveChar(const PkString &s, char16_t ch)
{
    PkString out;
    for (int i = 0; i < s.size(); ++i) {
        const char16_t c = s.at(i);
        if (c != ch) {
            out += pkCharToString(c);
        }
    }
    return out;
}

// 移除全部指定子串（对齐 Qt5 的 remove(str)）。
PkString pkRemoveSubstr(const PkString &s, const PkString &sub)
{
    return pkStringReplaceAll(s, sub, PkString(), PkCaseSensitive);
}

} // namespace

double SvgUtil::fromUserSpace(double value)
{
    return value;
}

double SvgUtil::toUserSpace(double value)
{
    return value;
}

double SvgUtil::ptToPx(SvgGraphicsContext *gc, double value)
{
    return value * gc->pixelsPerInch / DPI;
}

PkPointF SvgUtil::toUserSpace(const PkPointF &point)
{
    return PkPointF(toUserSpace(point.x()), toUserSpace(point.y()));
}

PkRectF SvgUtil::toUserSpace(const PkRectF &rect)
{
    return PkRectF(toUserSpace(rect.topLeft()), toUserSpace(rect.size()));
}

PkSizeF SvgUtil::toUserSpace(const PkSizeF &size)
{
    return PkSizeF(toUserSpace(size.width()), toUserSpace(size.height()));
}

PkString SvgUtil::toPercentage(qreal value)
{
    return KisDomUtils::toString(value * 100.0) + "%";
}

double SvgUtil::fromPercentage(PkString s, bool *ok)
{
    if (pkEndsWith(s, PkString("%")))
        return KisDomUtils::toDouble(pkRemoveChar(s, u'%'), ok) / 100.0;
    else
        return KisDomUtils::toDouble(s, ok);
}

PkPointF SvgUtil::objectToUserSpace(const PkPointF &position, const PkRectF &objectBound)
{
    qreal x = objectBound.left() + position.x() * objectBound.width();
    qreal y = objectBound.top() + position.y() * objectBound.height();
    return PkPointF(x, y);
}

PkSizeF SvgUtil::objectToUserSpace(const PkSizeF &size, const PkRectF &objectBound)
{
    qreal w = size.width() * objectBound.width();
    qreal h = size.height() * objectBound.height();
    return PkSizeF(w, h);
}

PkPointF SvgUtil::userSpaceToObject(const PkPointF &position, const PkRectF &objectBound)
{
    qreal x = 0.0;
    if (objectBound.width() != 0)
        x = (position.x() - objectBound.x()) / objectBound.width();
    qreal y = 0.0;
    if (objectBound.height() != 0)
        y = (position.y() - objectBound.y()) / objectBound.height();
    return PkPointF(x, y);
}

PkSizeF SvgUtil::userSpaceToObject(const PkSizeF &size, const PkRectF &objectBound)
{
    qreal w = objectBound.width() != 0 ? size.width() / objectBound.width() : 0.0;
    qreal h = objectBound.height() != 0 ? size.height() / objectBound.height() : 0.0;
    return PkSizeF(w, h);
}

PkString SvgUtil::transformToString(const PkTransform &transform)
{
    if (transform.isIdentity())
        return PkString();

    if (transform.type() == PkTransform::TxTranslate) {
        return PkString("translate(%1, %2)")
                     .arg(KisDomUtils::toString(toUserSpace(transform.dx())))
                     .arg(KisDomUtils::toString(toUserSpace(transform.dy())));
    } else {
        return PkString("matrix(%1 %2 %3 %4 %5 %6)")
                     .arg(KisDomUtils::toString(transform.m11()))
                     .arg(KisDomUtils::toString(transform.m12()))
                     .arg(KisDomUtils::toString(transform.m21()))
                     .arg(KisDomUtils::toString(transform.m22()))
                     .arg(KisDomUtils::toString(toUserSpace(transform.dx())))
                     .arg(KisDomUtils::toString(toUserSpace(transform.dy())));
    }
}

void SvgUtil::writeTransformAttributeLazy(const PkString &name, const PkTransform &transform, KoXmlWriter &shapeWriter)
{
    const PkString value = transformToString(transform);

    if (!value.isEmpty()) {
        const std::string nameUtf8 = name.PkToUtf8();
        shapeWriter.addAttribute(nameUtf8.c_str(), value);
    }
}

bool SvgUtil::parseViewBox(const PkXmlElement &e,
                           const PkRectF &elementBounds,
                           PkRectF *_viewRect, PkTransform *_viewTransform)
{
    KIS_ASSERT(_viewRect);
    KIS_ASSERT(_viewTransform);

    PkString viewBoxStr = e.attribute("viewBox");
    if (viewBoxStr.isEmpty()) return false;

    bool result = false;

    PkRectF viewBoxRect;
    // this is a workaround for bug 260429 for a file generated by blender
    // who has px in the viewbox which is wrong.
    // reported as bug https://developer.blender.org/T30971
    viewBoxStr = pkRemoveSubstr(viewBoxStr, PkString("px"));

    PkStringList points = pkSplitSkipEmpty(pkReplaceChar(viewBoxStr, u',', u' '), u' ');
    if (points.count() == 4) {
        viewBoxRect.setX(SvgUtil::fromUserSpace(points[0].toDouble()));
        viewBoxRect.setY(SvgUtil::fromUserSpace(points[1].toDouble()));
        viewBoxRect.setWidth(SvgUtil::fromUserSpace(points[2].toDouble()));
        viewBoxRect.setHeight(SvgUtil::fromUserSpace(points[3].toDouble()));

        result = true;
    } else {
        // TODO: WARNING!
    }

    if (!result) return false;

    qreal scaleX = 1;
    if (!qFuzzyCompare(elementBounds.width(), viewBoxRect.width())) {
        scaleX = elementBounds.width() / viewBoxRect.width();
    }
    qreal scaleY = 1;
    if (!qFuzzyCompare(elementBounds.height(), viewBoxRect.height())) {
        scaleY = elementBounds.height() / viewBoxRect.height();
    }

    PkTransform viewBoxTransform =
        PkTransform::fromTranslate(-viewBoxRect.x(), -viewBoxRect.y()) *
        PkTransform::fromScale(scaleX, scaleY) *
        PkTransform::fromTranslate(elementBounds.x(), elementBounds.y());

    const PkString aspectString = e.attribute("preserveAspectRatio");
    // give initial value if value not defined
    PreserveAspectRatioParser p( (!aspectString.isEmpty())? aspectString : PkString("xMidYMid meet"));
    parseAspectRatio(p, elementBounds, viewBoxRect, &viewBoxTransform);

    *_viewRect = viewBoxRect;
    *_viewTransform = viewBoxTransform;

    return result;
}

void SvgUtil::parseAspectRatio(const PreserveAspectRatioParser &p, const PkRectF &elementBounds, const PkRectF &viewBoxRect, PkTransform *_viewTransform)
{
    if (p.mode != Qt::IgnoreAspectRatio) {
        PkTransform viewBoxTransform = *_viewTransform;

        const qreal tan1 = viewBoxRect.height() / viewBoxRect.width();
        const qreal tan2 = elementBounds.height() / elementBounds.width();

        const qreal uniformScale =
            (p.mode == Qt::KeepAspectRatioByExpanding) ^ (tan1 > tan2) ?
                elementBounds.height() / viewBoxRect.height() :
                elementBounds.width() / viewBoxRect.width();

        viewBoxTransform =
            PkTransform::fromTranslate(-viewBoxRect.x(), -viewBoxRect.y()) *
            PkTransform::fromScale(uniformScale, uniformScale) *
            PkTransform::fromTranslate(elementBounds.x(), elementBounds.y());

        const PkPointF viewBoxAnchor = viewBoxTransform.map(p.rectAnchorPoint(viewBoxRect));
        const PkPointF elementAnchor = p.rectAnchorPoint(elementBounds);
        const PkPointF offset = elementAnchor - viewBoxAnchor;

        viewBoxTransform = viewBoxTransform * PkTransform::fromTranslate(offset.x(), offset.y());

        *_viewTransform = viewBoxTransform;
    }
}

qreal SvgUtil::parseUnit(SvgGraphicsContext *gc, const KoSvgTextProperties &resolved, const PkString &unit, bool horiz, bool vert, const PkRectF &bbox)
{
    if (unit.isEmpty())
        return 0.0;
    const std::string unitLatin1 = unit.PkToUtf8();
    // TODO : percentage?
    const char *start = unitLatin1.c_str();
    if (!start) {
        return 0.0;
    }
    KoSvgText::CssLengthPercentage length = parseUnitStruct(gc, unit, horiz, vert, bbox);
    length.convertToAbsolute(resolved.metrics(), resolved.fontSize().value);

    return length.value;
}

KoSvgText::CssLengthPercentage SvgUtil::parseUnitStruct(SvgGraphicsContext *gc, const PkString &unit, bool horiz, bool vert, const PkRectF &bbox)
{
    return parseUnitStructImpl(gc, unit, horiz, vert, bbox, true);
}

KoSvgText::CssLengthPercentage SvgUtil::parseTextUnitStruct(SvgGraphicsContext *gc, const PkString &unit)
{
    return parseUnitStructImpl(gc, unit, false, false, PkRectF(), false);
}

KoSvgText::CssLengthPercentage SvgUtil::parseUnitStructImpl(SvgGraphicsContext *gc, const PkString &unit, bool horiz, bool vert, const PkRectF &bbox, bool percentageViewBox)
{
    KoSvgText::CssLengthPercentage length;

    if (unit.isEmpty())
        return length;
    const std::string unitLatin1 = unit.trimmed().PkToUtf8();
    // TODO : percentage?
    const char *start = unitLatin1.c_str();
    if (!start) {
        return length;
    }
    const char *end = parseNumber(start, length.value);

    if (int(end - start) < unit.size()) {
        if (unit.right(2) == "px")
            length.value = SvgUtil::fromUserSpace(length.value);
        else if (unit.right(2) == "pt")
            length.value = ptToPx(gc, length.value);
        else if (unit.right(2) == "cm")
            length.value = ptToPx(gc, CM_TO_POINT(length.value));
        else if (unit.right(2) == "pc")
            length.value = ptToPx(gc, PI_TO_POINT(length.value));
        else if (unit.right(2) == "mm")
            length.value = ptToPx(gc, MM_TO_POINT(length.value));
        else if (unit.right(2) == "in")
            length.value = ptToPx(gc, INCH_TO_POINT(length.value));
        else if (unit.right(2) == "em") {
            length.unit = KoSvgText::CssLengthPercentage::Em;
        } else if (unit.right(2) == "ex") {
            length.unit = KoSvgText::CssLengthPercentage::Ex;
        } else if (unit.right(3) == "cap") {
            length.unit = KoSvgText::CssLengthPercentage::Cap;
        } else if (unit.right(2) == "ch") {
            length.unit = KoSvgText::CssLengthPercentage::Ch;
        } else if (unit.right(2) == "ic") {
            length.unit = KoSvgText::CssLengthPercentage::Ic;
        } else if (unit.right(2) == "lh") {
            length.unit = KoSvgText::CssLengthPercentage::Lh;
        } else if (unit.right(1) == "%") {

            if (percentageViewBox) {
                if (horiz && vert)
                    length.value = (length.value / 100.0) * (sqrt(pow(bbox.width(), 2) + pow(bbox.height(), 2)) / sqrt(2.0));
                else if (horiz)
                    length.value = (length.value / 100.0) * bbox.width();
                else if (vert)
                    length.value = (length.value / 100.0) * bbox.height();
            } else {
                length.value = (length.value / 100.0);
                length.unit = KoSvgText::CssLengthPercentage::Percentage;
            }
        }
    } else {
        length.value = SvgUtil::fromUserSpace(length.value);
    }

    return length;
}

qreal SvgUtil::parseUnitX(SvgGraphicsContext *gc, const KoSvgTextProperties &resolved, const PkString &unit)
{
    if (gc->forcePercentage) {
        return SvgUtil::fromPercentage(unit) * gc->currentBoundingBox.width();
    } else {
        return SvgUtil::parseUnit(gc, resolved, unit, true, false, gc->currentBoundingBox);
    }
}

qreal SvgUtil::parseUnitY(SvgGraphicsContext *gc, const KoSvgTextProperties &resolved, const PkString &unit)
{
    if (gc->forcePercentage) {
        return SvgUtil::fromPercentage(unit) * gc->currentBoundingBox.height();
    } else {
        return SvgUtil::parseUnit(gc, resolved, unit, false, true, gc->currentBoundingBox);
    }
}

qreal SvgUtil::parseUnitXY(SvgGraphicsContext *gc, const KoSvgTextProperties &resolved, const PkString &unit)
{
    if (gc->forcePercentage) {
        const qreal value = SvgUtil::fromPercentage(unit);
        return value * sqrt(pow(gc->currentBoundingBox.width(), 2) + pow(gc->currentBoundingBox.height(), 2)) / sqrt(2.0);
    } else {
        return SvgUtil::parseUnit(gc, resolved, unit, true, true, gc->currentBoundingBox);
    }
}

qreal SvgUtil::parseUnitAngular(SvgGraphicsContext *gc, const PkString &unit)
{
    Q_UNUSED(gc);

    qreal value = 0.0;

    if (unit.isEmpty()) return value;
    const std::string unitLatin1 = unit.toLower().PkToUtf8();

    const char *start = unitLatin1.c_str();
    if (!start) return value;

    const char *end = parseNumber(start, value);

    if (int(end - start) < unit.size()) {
        if (unit.right(3) == "deg") {
            value = kisDegreesToRadians(value);
        } else if (unit.right(4) == "grad") {
            value *= M_PI / 200;
        } else if (unit.right(3) == "rad") {
            // noop!
        } else {
            value = kisDegreesToRadians(value);
        }
    } else {
        value = kisDegreesToRadians(value);
    }

    return value;
}

qreal SvgUtil::parseNumber(const PkString &string)
{
    qreal value = 0.0;

    if (string.isEmpty()) return value;
    const std::string unitLatin1 = string.PkToUtf8();

    const char *start = unitLatin1.c_str();
    if (!start) return value;

    const char *end = parseNumber(start, value);
    KIS_SAFE_ASSERT_RECOVER_NOOP(int(end - start) == string.size());
    return value;
}

const char * SvgUtil::parseNumber(const char *ptr, qreal &number)
{
    int integer, exponent;
    qreal decimal, frac;
    int sign, expsign;

    exponent = 0;
    integer = 0;
    frac = 1.0;
    decimal = 0;
    sign = 1;
    expsign = 1;

    // read the sign
    if (*ptr == '+') {
        ptr++;
    } else if (*ptr == '-') {
        ptr++;
        sign = -1;
    }

    // read the integer part
    while (*ptr != '\0' && *ptr >= '0' && *ptr <= '9')
        integer = (integer * 10) + *(ptr++) - '0';
    if (*ptr == '.') { // read the decimals
        ptr++;
        while (*ptr != '\0' && *ptr >= '0' && *ptr <= '9')
            decimal += (*(ptr++) - '0') * (frac *= 0.1);
    }

    if (*ptr == 'e' || *ptr == 'E') { // read the exponent part
        ptr++;

        // read the sign of the exponent
        if (*ptr == '+') {
            ptr++;
        } else if (*ptr == '-') {
            ptr++;
            expsign = -1;
        }

        exponent = 0;
        while (*ptr != '\0' && *ptr >= '0' && *ptr <= '9') {
            exponent *= 10;
            exponent += *ptr - '0';
            ptr++;
        }
    }
    number = integer + decimal;
    number *= sign * pow((double)10, double(expsign * exponent));

    return ptr;
}

PkString SvgUtil::mapExtendedShapeTag(const PkString &tagName, const PkXmlElement &element)
{
    PkString result = tagName;

    if (tagName == "path") {
        PkString kritaType = element.attribute("krita:type", "");
        PkString sodipodiType = element.attribute("sodipodi:type", "");

        if (kritaType == "arc") {
            result = "krita:arc";
        } else if (sodipodiType == "arc") {
            result = "sodipodi:arc";
        }
    }

    return result;
}

PkStringList SvgUtil::simplifyList(const PkString &str)
{
    PkString attribute = str;
    attribute = pkReplaceChar(attribute, u',', u' ');
    attribute = pkRemoveChar(attribute, u'\r');
    attribute = pkRemoveChar(attribute, u'\n');
    return pkSplitSkipEmpty(pkSimplified(attribute), u' ');
}

SvgUtil::PreserveAspectRatioParser::PreserveAspectRatioParser(const PkString &str)
{
    std::regex rexp("(defer)?\\s*(none|(x(Min|Max|Mid)Y(Min|Max|Mid)))\\s*(meet|slice)?", std::regex::icase);
    std::smatch match;
    const std::string lowered = str.toLower().PkToUtf8();

    if (std::regex_search(lowered, match, rexp)) {
        if (match[1].str() == "defer") {
            defer = true;
        }

        if (match[2].str() != "none") {
            xAlignment = alignmentFromString(PkString(match[4].str().c_str()));
            yAlignment = alignmentFromString(PkString(match[5].str().c_str()));
            mode = match[6].str() == "slice" ?
                Qt::KeepAspectRatioByExpanding : Qt::KeepAspectRatio;
        }
    }
}

PkPointF SvgUtil::PreserveAspectRatioParser::rectAnchorPoint(const PkRectF &rc) const
{
    return PkPointF(alignedValue(rc.x(), rc.x() + rc.width(), xAlignment),
                    alignedValue(rc.y(), rc.y() + rc.height(), yAlignment));
}

PkString SvgUtil::PreserveAspectRatioParser::toString() const
{
    PkString result;

    if (!defer &&
        xAlignment == Middle &&
        yAlignment == Middle &&
        mode == Qt::KeepAspectRatio) {

        return result;
    }

    if (defer) {
        result += "defer ";
    }

    if (mode == Qt::IgnoreAspectRatio) {
        result += "none";
    } else {
        result += PkString("x%1Y%2")
            .arg(alignmentToString(xAlignment))
            .arg(alignmentToString(yAlignment));

        if (mode == Qt::KeepAspectRatioByExpanding) {
            result += " slice";
        }
    }

    return result;
}

SvgUtil::PreserveAspectRatioParser::Alignment SvgUtil::PreserveAspectRatioParser::alignmentFromString(const PkString &str) const {
    return
        str == "max" ? Max :
        str == "mid" ? Middle : Min;
}

PkString SvgUtil::PreserveAspectRatioParser::alignmentToString(SvgUtil::PreserveAspectRatioParser::Alignment alignment) const
{
    return
        alignment == Max ? "Max" :
        alignment == Min ? "Min" :
        "Mid";

}

qreal SvgUtil::PreserveAspectRatioParser::alignedValue(qreal min, qreal max, SvgUtil::PreserveAspectRatioParser::Alignment alignment)
{
    qreal result = min;

    switch (alignment) {
    case Min:
        result = min;
        break;
    case Middle:
        result = 0.5 * (min + max);
        break;
    case Max:
        result = max;
        break;
    }

    return result;
}
