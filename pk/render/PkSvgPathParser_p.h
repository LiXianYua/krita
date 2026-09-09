/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2007 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#pragma once

#include <PkString.h>
#include <PkByteArray.h>
#include <PkPoint.h>
#include <cassert>
#include <algorithm>
#include <cmath>
#include <string>

enum class PkSvgArcPolicy {
    Krita,
    QtSvg
};

// Extracted KoPathShapeLoader algorithm; Path supplies clear/moveTo/lineTo/curveTo/closeMerge.
template<class Path>
class PkSvgPathParser
{
public:
    PkSvgPathParser(Path *p, PkSvgArcPolicy arcPolicy)
        : path(p)
        , arcPolicy(arcPolicy)
    {
        assert(path);
        path->clear();
    }

    void parseSvg(const PkString &svgInputData, bool process = false);

    void svgMoveTo(qreal x1, qreal y1, bool abs = true);
    void svgLineTo(qreal x1, qreal y1, bool abs = true);
    void svgLineToHorizontal(qreal x, bool abs = true);
    void svgLineToVertical(qreal y, bool abs = true);
    void svgCurveToCubic(qreal x1, qreal y1, qreal x2, qreal y2, qreal x, qreal y, bool abs = true);
    void svgCurveToCubicSmooth(qreal x, qreal y, qreal x2, qreal y2, bool abs = true);
    void svgCurveToQuadratic(qreal x, qreal y, qreal x1, qreal y1, bool abs = true);
    void svgCurveToQuadraticSmooth(qreal x, qreal y, bool abs = true);
    void svgArcTo(qreal x, qreal y, qreal r1, qreal r2, qreal angle, bool largeArcFlag, bool sweepFlag, bool abs = true);
    void svgClosePath();

    const char *getCoord(const char *, qreal &);
    const char *getFlag(const char *ptr, bool &flag);
    void calculateArc(bool relative, qreal &curx, qreal &cury, qreal angle, qreal x, qreal y, qreal r1, qreal r2, bool largeArcFlag, bool sweepFlag);
    void calculateKritaArc(bool relative, qreal &curx, qreal &cury, qreal angle, qreal x, qreal y, qreal r1, qreal r2, bool largeArcFlag, bool sweepFlag);
    void calculateQtSvgArc(bool relative, qreal &curx, qreal &cury, qreal angle, qreal x, qreal y, qreal r1, qreal r2, bool largeArcFlag, bool sweepFlag);

    static qreal angleBetweenVectors(const PkPointF &a, const PkPointF &b)
    { return std::atan2(b.y(), b.x()) - std::atan2(a.y(), a.x()); }

    Path * path; ///< the path shape to work on
    PkSvgArcPolicy arcPolicy;
    PkPointF lastPoint;
};

template<class Path>
void PkSvgPathParser<Path>::parseSvg(const PkString &s, bool process)
{
    if (!s.isEmpty()) {
        PkString d = s;
        d.replace(',', ' ');
        d = d.simplified();

        const PkByteArray bytes = d.toLatin1();
        // PkByteArray deliberately owns bytes only, without a sentinel. The
        // original loader's numeric scanner requires a terminating NUL.
        const std::string buffer(bytes.constData(), bytes.size());
        const char *ptr = buffer.c_str();
        const char *end = ptr + buffer.size() + 1;

        qreal curx = 0.0;
        qreal cury = 0.0;
        qreal contrlx, contrly, subpathx, subpathy, tox, toy, x1, y1, x2, y2, xc, yc;
        qreal px1, py1, px2, py2, px3, py3;
        bool relative;
        char command = *(ptr++), lastCommand = ' ';

        subpathx = subpathy = curx = cury = contrlx = contrly = 0.0;
        while (ptr < end) {
            if (*ptr == ' ')
                ++ptr;

            relative = false;

            switch (command) {
            case 'm':
                relative = true;
                [[fallthrough]];
            case 'M': {
                ptr = getCoord(ptr, tox);
                ptr = getCoord(ptr, toy);

                if (process) {
                    subpathx = curx = relative ? curx + tox : tox;
                    subpathy = cury = relative ? cury + toy : toy;

                    svgMoveTo(curx, cury);
                } else
                    svgMoveTo(tox, toy, !relative);
                break;
            }
            case 'l':
                relative = true;
                [[fallthrough]];
            case 'L': {
                ptr = getCoord(ptr, tox);
                ptr = getCoord(ptr, toy);

                if (process) {
                    curx = relative ? curx + tox : tox;
                    cury = relative ? cury + toy : toy;

                    svgLineTo(curx, cury);
                } else
                    svgLineTo(tox, toy, !relative);
                break;
            }
            case 'h': {
                ptr = getCoord(ptr, tox);
                if (process) {
                    curx = curx + tox;
                    svgLineTo(curx, cury);
                } else
                    svgLineToHorizontal(tox, false);
                break;
            }
            case 'H': {
                ptr = getCoord(ptr, tox);
                if (process) {
                    curx = tox;
                    svgLineTo(curx, cury);
                } else
                    svgLineToHorizontal(tox);
                break;
            }
            case 'v': {
                ptr = getCoord(ptr, toy);
                if (process) {
                    cury = cury + toy;
                    svgLineTo(curx, cury);
                } else
                    svgLineToVertical(toy, false);
                break;
            }
            case 'V': {
                ptr = getCoord(ptr, toy);
                if (process) {
                    cury = toy;
                    svgLineTo(curx, cury);
                } else
                    svgLineToVertical(toy);
                break;
            }
            case 'z':
                [[fallthrough]];
            case 'Z': {
                // reset curx, cury for next path
                if (process) {
                    curx = subpathx;
                    cury = subpathy;
                }
                svgClosePath();
                break;
            }
            case 'c':
                relative = true;
                [[fallthrough]];
            case 'C': {
                ptr = getCoord(ptr, x1);
                ptr = getCoord(ptr, y1);
                ptr = getCoord(ptr, x2);
                ptr = getCoord(ptr, y2);
                ptr = getCoord(ptr, tox);
                ptr = getCoord(ptr, toy);

                if (process) {
                    px1 = relative ? curx + x1 : x1;
                    py1 = relative ? cury + y1 : y1;
                    px2 = relative ? curx + x2 : x2;
                    py2 = relative ? cury + y2 : y2;
                    px3 = relative ? curx + tox : tox;
                    py3 = relative ? cury + toy : toy;

                    svgCurveToCubic(px1, py1, px2, py2, px3, py3);

                    contrlx = relative ? curx + x2 : x2;
                    contrly = relative ? cury + y2 : y2;
                    curx = relative ? curx + tox : tox;
                    cury = relative ? cury + toy : toy;
                } else
                    svgCurveToCubic(x1, y1, x2, y2, tox, toy, !relative);

                break;
            }
            case 's':
                relative = true;
                [[fallthrough]];
            case 'S': {
                ptr = getCoord(ptr, x2);
                ptr = getCoord(ptr, y2);
                ptr = getCoord(ptr, tox);
                ptr = getCoord(ptr, toy);
                if (!(lastCommand == 'c' || lastCommand == 'C' ||
                        lastCommand == 's' || lastCommand == 'S')) {
                    contrlx = curx;
                    contrly = cury;
                }

                if (process) {
                    px1 = 2 * curx - contrlx;
                    py1 = 2 * cury - contrly;
                    px2 = relative ? curx + x2 : x2;
                    py2 = relative ? cury + y2 : y2;
                    px3 = relative ? curx + tox : tox;
                    py3 = relative ? cury + toy : toy;

                    svgCurveToCubic(px1, py1, px2, py2, px3, py3);

                    contrlx = relative ? curx + x2 : x2;
                    contrly = relative ? cury + y2 : y2;
                    curx = relative ? curx + tox : tox;
                    cury = relative ? cury + toy : toy;
                } else
                    svgCurveToCubicSmooth(x2, y2, tox, toy, !relative);
                break;
            }
            case 'q':
                relative = true;
                [[fallthrough]];
            case 'Q': {
                ptr = getCoord(ptr, x1);
                ptr = getCoord(ptr, y1);
                ptr = getCoord(ptr, tox);
                ptr = getCoord(ptr, toy);

                if (process) {
                    px1 = relative ? (curx + 2 * (x1 + curx)) * (1.0 / 3.0) : (curx + 2 * x1) * (1.0 / 3.0);
                    py1 = relative ? (cury + 2 * (y1 + cury)) * (1.0 / 3.0) : (cury + 2 * y1) * (1.0 / 3.0);
                    px2 = relative ? ((curx + tox) + 2 * (x1 + curx)) * (1.0 / 3.0) : (tox + 2 * x1) * (1.0 / 3.0);
                    py2 = relative ? ((cury + toy) + 2 * (y1 + cury)) * (1.0 / 3.0) : (toy + 2 * y1) * (1.0 / 3.0);
                    px3 = relative ? curx + tox : tox;
                    py3 = relative ? cury + toy : toy;

                    svgCurveToCubic(px1, py1, px2, py2, px3, py3);

                    contrlx = relative ? curx + x1 : x1;
                    contrly = relative ? cury + y1 : y1;
                    curx = relative ? curx + tox : tox;
                    cury = relative ? cury + toy : toy;
                } else
                    svgCurveToQuadratic(x1, y1, tox, toy, !relative);
                break;
            }
            case 't':
                relative = true;
                [[fallthrough]];
            case 'T': {
                ptr = getCoord(ptr, tox);
                ptr = getCoord(ptr, toy);
                if (!(lastCommand == 'q' || lastCommand == 'Q' ||
                        lastCommand == 't' || lastCommand == 'T')) {
                    contrlx = curx;
                    contrly = cury;
                }

                if (process) {
                    xc = 2 * curx - contrlx;
                    yc = 2 * cury - contrly;

                    px1 = (curx + 2 * xc) * (1.0 / 3.0);
                    py1 = (cury + 2 * yc) * (1.0 / 3.0);
                    px2 = relative ? ((curx + tox) + 2 * xc) * (1.0 / 3.0) : (tox + 2 * xc) * (1.0 / 3.0);
                    py2 = relative ? ((cury + toy) + 2 * yc) * (1.0 / 3.0) : (toy + 2 * yc) * (1.0 / 3.0);
                    px3 = relative ? curx + tox : tox;
                    py3 = relative ? cury + toy : toy;

                    svgCurveToCubic(px1, py1, px2, py2, px3, py3);

                    contrlx = xc;
                    contrly = yc;
                    curx = relative ? curx + tox : tox;
                    cury = relative ? cury + toy : toy;
                } else
                    svgCurveToQuadraticSmooth(tox, toy, !relative);
                break;
            }
            case 'a':
                relative = true;
                [[fallthrough]];
            case 'A': {
                bool largeArc = false;
                bool sweep = false;
                qreal angle, rx, ry;
                ptr = getCoord(ptr, rx);
                ptr = getCoord(ptr, ry);
                ptr = getCoord(ptr, angle);
                ptr = getFlag(ptr, largeArc);
                ptr = getFlag(ptr, sweep);
                ptr = getCoord(ptr, tox);
                ptr = getCoord(ptr, toy);

                // Spec: radii are nonnegative numbers
                rx = fabs(rx);
                ry = fabs(ry);

                if (process)
                    calculateArc(relative, curx, cury, angle, tox, toy, rx, ry, largeArc, sweep);
                else
                    svgArcTo(tox, toy, rx, ry, angle, largeArc, sweep, !relative);
                break;
            }
            default: {
                // when svg parser is used for a parsing an odf path an unknown command
                // can be encountered, so we stop parsing here
                return;
            }
            }

            lastCommand = command;

            if (*ptr == '+' || *ptr == '-' || *ptr == '.' || (*ptr >= '0' && *ptr <= '9')) {
                if (command == 'z' || command == 'Z') return;
                // there are still coords in this command
                if (command == 'M')
                    command = 'L';
                else if (command == 'm')
                    command = 'l';
            } else
                command = *(ptr++);

            if (lastCommand != 'C' && lastCommand != 'c' &&
                    lastCommand != 'S' && lastCommand != 's' &&
                    lastCommand != 'Q' && lastCommand != 'q' &&
                    lastCommand != 'T' && lastCommand != 't') {
                contrlx = curx;
                contrly = cury;
            }
        }
    }
}

// parses the coord into number and forwards to the next token
template<class Path>
const char * PkSvgPathParser<Path>::getCoord(const char *ptr, qreal &number)
{
    qreal integer;
    int exponent;
    qreal decimal, frac;
    int sign, expsign;

    exponent = 0;
    integer = 0;
    frac = 1.0;
    decimal = 0;
    sign = 1;
    expsign = 1;

    // read the sign
    if (*ptr == '+')
        ++ptr;
    else if (*ptr == '-') {
        ++ptr;
        sign = -1;
    }

    // read the integer part
    while (*ptr != '\0' && *ptr >= '0' && *ptr <= '9')
        integer = (integer * 10) + *(ptr++) - '0';
    if (*ptr == '.') { // read the decimals
        ++ptr;
        while (*ptr != '\0' && *ptr >= '0' && *ptr <= '9')
            decimal += (*(ptr++) - '0') * (frac *= 0.1);
    }

    if (*ptr == 'e' || *ptr == 'E') { // read the exponent part
        ++ptr;

        // read the sign of the exponent
        if (*ptr == '+')
            ++ptr;
        else if (*ptr == '-') {
            ++ptr;
            expsign = -1;
        }

        exponent = 0;
        while (*ptr != '\0' && *ptr >= '0' && *ptr <= '9') {
            exponent = std::min(10000, exponent * 10 + *ptr - '0');
            ++ptr;
        }
    }
    number = integer + decimal;
    number *= sign * pow((qreal)10, qreal(expsign * exponent));

    // skip the following space
    if (*ptr == ' ')
        ++ptr;

    return ptr;
}

template<class Path>
const char * PkSvgPathParser<Path>::getFlag(const char *ptr, bool &flag)
{
    // check for '0' or '1'
    if (*ptr != '0' && *ptr != '1') {
        return ptr;
    }
    flag = (*ptr == '1');
    ++ptr;

    if (*ptr == ' ') {
        ++ptr;
    }
    return ptr;
}

template<class Path>
void PkSvgPathParser<Path>::calculateArc(bool relative, qreal &curx, qreal &cury, qreal angle, qreal x, qreal y, qreal rx, qreal ry, bool largeArcFlag, bool sweepFlag)
{
    if (arcPolicy == PkSvgArcPolicy::QtSvg) {
        calculateQtSvgArc(relative, curx, cury, angle, x, y, rx, ry, largeArcFlag, sweepFlag);
    } else {
        calculateKritaArc(relative, curx, cury, angle, x, y, rx, ry, largeArcFlag, sweepFlag);
    }
}

// Krita/flake's original SVG-to-cubic conversion. It intentionally uses
// at-most-45-degree segments and represents zero-radius arcs as lines.
template<class Path>
void PkSvgPathParser<Path>::calculateKritaArc(bool relative, qreal &curx, qreal &cury, qreal angle, qreal x, qreal y, qreal rx, qreal ry, bool largeArcFlag, bool sweepFlag)
{
    if (pkQtFuzzyCompare(rx, 0.0) || pkQtFuzzyCompare(ry, 0.0)
        || (!relative && pkQtFuzzyCompare(curx - x, 0) && pkQtFuzzyCompare(cury - y, 0))
        || (relative && pkQtFuzzyCompare(x, 0) && pkQtFuzzyCompare(y, 0))) {
        qreal x2 = x;
        qreal y2 = y;

        if (relative) {
            x2 += curx;
            y2 += cury;
        }
        svgLineTo(x2, y2);
        return;
    }

    const qreal angleRadians = angle * (M_PI / 180.0);
    const qreal sinTh = std::sin(angleRadians);
    const qreal cosTh = std::cos(angleRadians);

    qreal dx;
    qreal x2 = x;
    if (!relative) {
        dx = (curx - x) / 2.0;
    } else {
        dx = -(x / 2.0);
        x2 = curx + x;
    }

    qreal dy;
    qreal y2 = y;
    if (!relative) {
        dy = (cury - y) / 2.0;
    } else {
        dy = -(y / 2.0);
        y2 = cury + y;
    }

    const qreal x1Prime = cosTh * dx + sinTh * dy;
    const qreal y1Prime = -sinTh * dx + cosTh * dy;
    const qreal x1PrimeSquared = x1Prime * x1Prime;
    const qreal y1PrimeSquared = y1Prime * y1Prime;
    qreal radiusXSquared = rx * rx;
    qreal radiusYSquared = ry * ry;

    const qreal check = x1PrimeSquared / radiusXSquared + y1PrimeSquared / radiusYSquared;
    if (check > 1) {
        rx *= std::sqrt(check);
        ry *= std::sqrt(check);
        radiusXSquared = rx * rx;
        radiusYSquared = ry * ry;
    }

    const qreal radiiSquared = radiusXSquared * radiusYSquared;
    const qreal ellipseValue = radiusXSquared * y1PrimeSquared + radiusYSquared * x1PrimeSquared;
    qreal coefficient = std::sqrt(std::fabs((radiiSquared - ellipseValue) / ellipseValue));
    if (sweepFlag == largeArcFlag) {
        coefficient = -coefficient;
    }

    const qreal centerXPrime = coefficient * (rx * y1Prime) / ry;
    const qreal centerYPrime = coefficient * -(ry * x1Prime) / rx;
    const qreal centerX = cosTh * centerXPrime - sinTh * centerYPrime + (curx + x2) * 0.5;
    const qreal centerY = sinTh * centerXPrime + cosTh * centerYPrime + (cury + y2) * 0.5;

    const PkPointF startVector = {
        (x1Prime - centerXPrime) / rx,
        (y1Prime - centerYPrime) / ry
    };
    const qreal theta = angleBetweenVectors({1.0, 0.0}, startVector);
    qreal delta = std::fmod(
        angleBetweenVectors(startVector,
                            {(-x1Prime - centerXPrime) / rx,
                             (-y1Prime - centerYPrime) / ry}),
        M_PI * 2);

    if (sweepFlag && delta < 0) {
        delta += M_PI * 2;
    } else if (!sweepFlag && delta > 0) {
        delta -= M_PI * 2;
    }

    const int segments = int(std::ceil(std::fabs(delta / (M_PI * 0.25))));
    for (int i = 0; i < segments; ++i) {
        const qreal start = theta + i * delta / segments;
        const qreal end = theta + (i + 1) * delta / segments;
        const qreal half = 0.5 * (end - start);

        const auto ellipsePoint = [sinTh, cosTh](qreal cx, qreal cy, qreal eta, qreal radiusX, qreal radiusY) {
            return PkPointF(cx + radiusX * cosTh * std::cos(eta) - radiusY * sinTh * std::sin(eta),
                            cy + radiusX * sinTh * std::cos(eta) + radiusY * cosTh * std::sin(eta));
        };
        const auto ellipseDerivative = [sinTh, cosTh](qreal eta, qreal radiusX, qreal radiusY) {
            return PkPointF(-radiusX * cosTh * std::sin(eta) - radiusY * sinTh * std::cos(eta),
                            -radiusX * sinTh * std::sin(eta) + radiusY * cosTh * std::cos(eta));
        };

        const PkPointF p1 = ellipsePoint(centerX, centerY, start, rx, ry);
        const PkPointF p2 = ellipsePoint(centerX, centerY, end, rx, ry);
        const qreal alpha = std::sin(end - start)
            * (std::sqrt(4 + 3 * std::tan(half) * std::tan(half)) - 1) / 3;
        const PkPointF control1 = p1 + alpha * ellipseDerivative(start, rx, ry);
        const PkPointF control2 = p2 - alpha * ellipseDerivative(end, rx, ry);
        svgCurveToCubic(control1.x(), control1.y(), control2.x(), control2.y(), p2.x(), p2.y());
    }

    if (!relative) {
        curx = x;
        cury = y;
    } else {
        curx += x;
        cury += y;
    }
}

// QtSvg 5.15's pathArc()/pathArcSegment() conversion, originally from XSVG.
// SPDX-SnippetBegin
// SPDX-License-Identifier: BSD-3-Clause
// SPDX-SnippetCopyrightText: 2002 USC/Information Sciences Institute
template<class Path>
void PkSvgPathParser<Path>::calculateQtSvgArc(bool relative, qreal &curx, qreal &cury, qreal angle, qreal x, qreal y, qreal rx, qreal ry, bool largeArcFlag, bool sweepFlag)
{
    const qreal endX = relative ? curx + x : x;
    const qreal endY = relative ? cury + y : y;
    const qreal radiusXSquared = rx * rx;
    const qreal radiusYSquared = ry * ry;

    if (radiusXSquared && radiusYSquared) {
        rx = std::abs(rx);
        ry = std::abs(ry);

        const qreal angleRadians = angle * (M_PI / 180.0);
        const qreal sinAngle = std::sin(angleRadians);
        const qreal cosAngle = std::cos(angleRadians);
        const qreal dx = (curx - endX) / 2.0;
        const qreal dy = (cury - endY) / 2.0;
        const qreal rotatedX = cosAngle * dx + sinAngle * dy;
        const qreal rotatedY = -sinAngle * dx + cosAngle * dy;
        const qreal check = rotatedX * rotatedX / radiusXSquared
            + rotatedY * rotatedY / radiusYSquared;
        if (check > 1) {
            rx *= std::sqrt(check);
            ry *= std::sqrt(check);
        }

        const qreal a00 = cosAngle / rx;
        const qreal a01 = sinAngle / rx;
        const qreal a10 = -sinAngle / ry;
        const qreal a11 = cosAngle / ry;
        const qreal x0 = a00 * curx + a01 * cury;
        const qreal y0 = a10 * curx + a11 * cury;
        const qreal x1 = a00 * endX + a01 * endY;
        const qreal y1 = a10 * endX + a11 * endY;
        const qreal distance = (x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0);

        if (distance > 0 && std::isfinite(distance)) {
            const qreal factorSquared = std::max<qreal>(0, 1.0 / distance - 0.25);
            qreal factor = std::sqrt(factorSquared);
            if (sweepFlag == largeArcFlag) factor = -factor;
            const qreal centerX = 0.5 * (x0 + x1) - factor * (y1 - y0);
            const qreal centerY = 0.5 * (y0 + y1) + factor * (x1 - x0);
            const qreal theta0 = std::atan2(y0 - centerY, x0 - centerX);
            const qreal theta1 = std::atan2(y1 - centerY, x1 - centerX);
            qreal arc = theta1 - theta0;
            if (arc < 0 && sweepFlag) arc += 2 * M_PI;
            else if (arc > 0 && !sweepFlag) arc -= 2 * M_PI;
            const int segments = int(std::ceil(std::abs(arc / (M_PI * 0.5 + 0.001))));

            const qreal out00 = cosAngle * rx;
            const qreal out01 = -sinAngle * ry;
            const qreal out10 = sinAngle * rx;
            const qreal out11 = cosAngle * ry;
            for (int i = 0; i < segments; ++i) {
                const qreal start = theta0 + i * arc / segments;
                const qreal end = theta0 + (i + 1) * arc / segments;
                const qreal half = 0.5 * (end - start);
                const qreal tangent = (8.0 / 3.0) * std::sin(half * 0.5) * std::sin(half * 0.5) / std::sin(half);
                const qreal control1X = centerX + std::cos(start) - tangent * std::sin(start);
                const qreal control1Y = centerY + std::sin(start) + tangent * std::cos(start);
                const qreal pointX = centerX + std::cos(end);
                const qreal pointY = centerY + std::sin(end);
                const qreal control2X = pointX + tangent * std::sin(end);
                const qreal control2Y = pointY - tangent * std::cos(end);
                svgCurveToCubic(out00 * control1X + out01 * control1Y,
                                out10 * control1X + out11 * control1Y,
                                out00 * control2X + out01 * control2Y,
                                out10 * control2X + out11 * control2Y,
                                out00 * pointX + out01 * pointY,
                                out10 * pointX + out11 * pointY);
            }
        }
    }

    curx = endX;
    cury = endY;
}
// SPDX-SnippetEnd

template<class Path>
void PkSvgPathParser<Path>::svgMoveTo(qreal x1, qreal y1, bool abs)
{
    if (abs)
        lastPoint = PkPointF(x1, y1);
    else
        lastPoint += PkPointF(x1, y1);
    path->moveTo(lastPoint);
}

template<class Path>
void PkSvgPathParser<Path>::svgLineTo(qreal x1, qreal y1, bool abs)
{
    if (abs)
        lastPoint = PkPointF(x1, y1);
    else
        lastPoint += PkPointF(x1, y1);

    path->lineTo(lastPoint);
}

template<class Path>
void PkSvgPathParser<Path>::svgLineToHorizontal(qreal x, bool abs)
{
    if (abs)
        lastPoint.setX(x);
    else
        lastPoint.rx() += x;

    path->lineTo(lastPoint);
}

template<class Path>
void PkSvgPathParser<Path>::svgLineToVertical(qreal y, bool abs)
{
    if (abs)
        lastPoint.setY(y);
    else
        lastPoint.ry() += y;

    path->lineTo(lastPoint);
}

template<class Path>
void PkSvgPathParser<Path>::svgCurveToCubic(qreal x1, qreal y1, qreal x2, qreal y2, qreal x, qreal y, bool abs)
{
    PkPointF p1, p2;
    if (abs) {
        p1 = PkPointF(x1, y1);
        p2 = PkPointF(x2, y2);
        lastPoint = PkPointF(x, y);
    } else {
        p1 = lastPoint + PkPointF(x1, y1);
        p2 = lastPoint + PkPointF(x2, y2);
        lastPoint += PkPointF(x, y);
    }

    path->curveTo(p1, p2, lastPoint);
}

template<class Path>
void PkSvgPathParser<Path>::svgCurveToCubicSmooth(qreal x, qreal y, qreal x2, qreal y2, bool abs)
{
    (void)x;
    (void)y;
    (void)x2;
    (void)y2;
    (void)abs;
    // TODO implement
}

template<class Path>
void PkSvgPathParser<Path>::svgCurveToQuadratic(qreal x, qreal y, qreal x1, qreal y1, bool abs)
{
    (void)x;
    (void)y;
    (void)x1;
    (void)y1;
    (void)abs;
    // TODO implement
}

template<class Path>
void PkSvgPathParser<Path>::svgCurveToQuadraticSmooth(qreal x, qreal y, bool abs)
{
    (void)x;
    (void)y;
    (void)abs;
    // TODO implement
}

template<class Path>
void PkSvgPathParser<Path>::svgArcTo(qreal x, qreal y, qreal r1, qreal r2, qreal angle, bool largeArcFlag, bool sweepFlag, bool abs)
{
    (void)x;
    (void)y;
    (void)r1;
    (void)r2;
    (void)angle;
    (void)largeArcFlag;
    (void)sweepFlag;
    (void)abs;
    // TODO implement
}

template<class Path>
void PkSvgPathParser<Path>::svgClosePath()
{
    path->closeMerge();
}
