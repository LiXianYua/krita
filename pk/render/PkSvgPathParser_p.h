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

// Extracted KoPathShapeLoader algorithm; Path supplies clear/moveTo/lineTo/curveTo/closeMerge.
template<class Path>
class PkSvgPathParser
{
public:
    PkSvgPathParser(Path * p) : path(p) {
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

    static qreal angleBetweenVectors(const PkPointF &a, const PkPointF &b)
    { return std::atan2(b.y(), b.x()) - std::atan2(a.y(), a.x()); }

    Path * path; ///< the path shape to work on
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

// This works by converting the SVG arc to "simple" beziers.
// For each bezier found a svgToCurve call is done.
template<class Path>
void PkSvgPathParser<Path>::calculateArc(bool relative, qreal &curx, qreal &cury, qreal angle, qreal x, qreal y, qreal rx, qreal ry, bool largeArcFlag, bool sweepFlag)
{
    // if radii are zero or the endpoints of ellipse are same as its start point, then use lineTo/don't do
    // anything.
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
    const qreal sin_th = sin(angleRadians);
    const qreal cos_th = cos(angleRadians);

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

    // From SVG spec

    // Step 1: Compute (x1_prime, y1_prime)
    const qreal x1Prime = cos_th * dx + sin_th * dy;
    const qreal y1Prime = -sin_th * dx + cos_th * dy;

    // eq. 5.1
    const qreal x1PrimeSq = x1Prime * x1Prime;
    const qreal y1PrimeSq = y1Prime * y1Prime;

    // Step 2: Compute (c_x_prime, c_y_prime)
    qreal rxSq = rx * rx;
    qreal rySq = ry * ry;

    // Spec : check if radii are large enough
    // eq. 6.2
    const qreal check = x1PrimeSq / rxSq + y1PrimeSq / rySq;
    if (check > 1) {
        // eq. 6.3
        rx = rx * sqrt(check);
        ry = ry * sqrt(check);

        rxSq = rx * rx;
        rySq = ry * ry;
    }

    // Step 2: Compute (c_x_prime, c_y_prime)
    const qreal radiiSq = rxSq * rySq;
    const qreal ellipseValue = rxSq * y1PrimeSq + rySq * x1PrimeSq;

    qreal coef = sqrt(std::fabs((radiiSq - ellipseValue) / ellipseValue));
    if (sweepFlag == largeArcFlag) {
        coef = -coef;
    }
    // eq. 5.2
    const qreal cxPrime = coef * (rx * y1Prime) / ry;
    const qreal cyPrime = coef * -(ry * x1Prime) / rx;

    // Step 3: Compute (c_x, c_y) from (c_x_prime, c_y_prime)
    // eq. 5.3
    const qreal cx = cos_th * cxPrime - sin_th * cyPrime + (curx + x2) * 0.5;
    const qreal cy = sin_th * cxPrime + cos_th * cyPrime + (cury + y2) * 0.5;

    // Step 4: Compute angle and delta
    const PkPointF v = {(x1Prime - cxPrime) / rx, (y1Prime - cyPrime) / ry};
    // eq. 5.5
    const qreal theta = angleBetweenVectors({1.0, 0.0}, v);
    // eq. 5.6
    qreal delta = std::fmod(
        angleBetweenVectors(v, {(-x1Prime - cxPrime) / rx, (-y1Prime - cyPrime) / ry}),
        M_PI * 2);

    if (sweepFlag && delta < 0) {
        delta += (M_PI * 2);
    } else if (!sweepFlag && delta > 0) {
        delta -= (M_PI * 2);
    }

    int n_segs = (int)ceil(fabs(delta / (M_PI * 0.25)));

    // From: http://www.spaceroots.org/documents/ellipse/elliptical-arc.pdf
    for (int i = 0; i < n_segs; ++i) {
        const qreal eta1 = theta + i * delta / n_segs;
        const qreal eta2 = theta + (i + 1) * delta / n_segs;

        const qreal etaHalf = 0.5 * (eta2 - eta1);

        const qreal cosAngle = cos(angleRadians);
        const qreal sinAngle = sin(angleRadians);

        auto ellipseArcToPoint = [sinAngle, cosAngle](qreal cx, qreal cy, qreal eta, qreal rx, qreal ry) {
            qreal x = cx + (rx * cosAngle * cos(eta)) - (ry * sinAngle * sin(eta));
            qreal y = cy + (rx * sinAngle * cos(eta)) + (ry * cosAngle * sin(eta));
            return PkPointF(x, y);
        };
        auto ellipseDerivativeArcToPoint = [sinAngle, cosAngle](qreal eta, qreal rx, qreal ry) {
            qreal x = -(rx * cosAngle * sin(eta)) - (ry * sinAngle * cos(eta));
            qreal y = -(rx * sinAngle * sin(eta)) + (ry * cosAngle * cos(eta));
            return PkPointF(x, y);
        };

        // bezier control points
        const PkPointF p1 = ellipseArcToPoint(cx, cy, eta1, rx, ry);
        const PkPointF p2 = ellipseArcToPoint(cx, cy, eta2, rx, ry);

        const qreal alpha = sin(eta2 - eta1) * (sqrt(4 + 3 * tan(etaHalf) * tan(etaHalf)) - 1) / 3;

        const PkPointF q1 = p1 + alpha * ellipseDerivativeArcToPoint(eta1, rx, ry);
        const PkPointF q2 = p2 - alpha * ellipseDerivativeArcToPoint(eta2, rx, ry);

        svgCurveToCubic(q1.x(), q1.y(), q2.x(), q2.y(), p2.x(), p2.y());
    }

    if (!relative)
        curx = x;
    else
        curx += x;

    if (!relative)
        cury = y;
    else
        cury += y;
}

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
