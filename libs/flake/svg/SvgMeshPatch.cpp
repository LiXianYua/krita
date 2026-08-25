/*
 *  SPDX-FileCopyrightText: 2007 Jan Hambrecht <jaham@gmx.net>
 *  SPDX-FileCopyrightText: 2020 Sharaf Zaman <sharafzaz121@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <QtCore/QtCore>
#include <PkFlakeBridge.h>

#include <PkMessageLogger.h>
#include <algorithm>

#include "SvgMeshPatch.h"

#include <array>
#include <math.h>
#include <kis_assert.h>


namespace {

// 白空折叠为单空格并 trim（对齐 Qt5 的 simplified）。
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

// 将该 ASCII 字符全部替换为另一字符（对齐 Qt5 的 replace(ch, ch)）。
PkString pkReplaceChar(const PkString &s, char from, char to)
{
    const std::string in = s.PkToUtf8();
    std::string out;
    out.reserve(in.size());
    for (const char c : in) {
        out.push_back((c == from) ? to : c);
    }
    return PkString::PkFromUtf8(out.data(), static_cast<int>(out.size()));
}

}

inline PkPointF lerp(const PkPointF& p1, const PkPointF& p2, qreal t)
{
    return (1 - t) * p1 + t * p2;
}

void deCasteljau(const std::array<PkPointF, 4>& points,
                 qreal t, PkPointF *p1, PkPointF *p2,
                 PkPointF *p3, PkPointF *p4, PkPointF *p5)
{
    PkPointF q[4];

    q[0] = points[0];
    q[1] = points[1];
    q[2] = points[2];
    q[3] = points[3];

    // points of the new segment after the split point
    PkPointF p[3];

    // the De Casteljau algorithm
    for (unsigned short j = 1; j <= 3; ++j) {
        for (unsigned short i = 0; i <= 3 - j; ++i) {
            q[i] = (1.0 - t) * q[i] + t * q[i + 1];
        }
        p[j - 1] = q[0];
    }

    if (p1)
        *p1 = p[0];
    if (p2)
        *p2 = p[1];
    if (p3)
        *p3 = p[2];
    if (p4)
        *p4 = q[1];
    if (p5)
        *p5 = q[2];
}

PkPair<std::array<PkPointF, 4>, std::array<PkPointF, 4>> splitAt(const std::array<PkPointF, 4>& points, qreal t)
{
    PkPointF newCP2, newCP1, splitP, splitCP1, splitCP2;
    deCasteljau(points, t, &newCP2, &splitCP1, &splitP, &splitCP2, &newCP1);
    return {{points[0], newCP2, splitCP1, splitP},
            {splitP, splitCP2, newCP1, points[3]}};
}

SvgMeshPatch::SvgMeshPatch(PkPointF startingPoint)
    : m_newPath(true)
    , m_startingPoint(startingPoint)
    , m_parametricCoords({PkPointF(0, 0), {1, 0}, {1, 1}, {0, 1}})
{
}

SvgMeshPatch::SvgMeshPatch(const SvgMeshPatch& other)
    : m_newPath(other.m_newPath)
    , m_startingPoint(other.m_startingPoint)
    , m_nodes(other.m_nodes)
    , controlPoints(other.controlPoints)
    , m_parametricCoords({PkPointF(0, 0), {1, 0}, {1, 1}, {0, 1}})
{
}

void SvgMeshPatch::moveTo(const PkPointF& p)
{
    controlPoints[counter][0] = p;
}

void SvgMeshPatch::lineTo(const PkPointF& p)
{
    controlPoints[counter][1] = lerp(controlPoints[counter][0], p, 1.0 / 3);
    controlPoints[counter][2] = lerp(controlPoints[counter][0], p, 2.0 / 3);
    controlPoints[counter][3] = p;
    counter++;
    if (counter < Size)
        controlPoints[counter][0] = p;
}

void SvgMeshPatch::curveTo(const PkPointF& c1, const PkPointF& c2, const PkPointF& p)
{
    controlPoints[counter][1] = c1;
    controlPoints[counter][2] = c2;
    controlPoints[counter][3] = p;
    counter++;
    if (counter < Size)
        controlPoints[counter][0] = p;
}

SvgMeshStop SvgMeshPatch::getStop(SvgMeshPatch::Type type) const
{
    return m_nodes[type];
}

PkPointF SvgMeshPatch::segmentPointAt(Type type, qreal t) const
{
    PkPointF p;
    deCasteljau(controlPoints[type], t, 0, 0, &p, 0, 0);
    return p;
}

PkPair<std::array<PkPointF, 4>, std::array<PkPointF, 4>> SvgMeshPatch::segmentSplitAt(Type type, qreal t) const
{
    return splitAt(controlPoints[type], t);
}

std::array<PkPointF, 4> SvgMeshPatch::getSegment(Type type) const
{
    return controlPoints[type];
}

PkPainterPath SvgMeshPatch::getPath() const
{
    PkPainterPath path;
    path.moveTo(controlPoints[Top][0]);
    for (const auto& i: controlPoints) {
        path.cubicTo(i[1], i[2], i[3]);
    }
    return path;
}

PkRectF SvgMeshPatch::boundingRect() const
{
    return getPath().boundingRect();
}

PkSizeF SvgMeshPatch::size() const
{
    return boundingRect().size();
}

std::array<PkPointF, 4> SvgMeshPatch::getMidCurve(bool isVertical) const
{
    std::array<PkPointF, 4> p;
    std::array<PkPointF, 4> curvedBoundary0;
    std::array<PkPointF, 4> curvedBoundary1;

    PkPointF midpointRuled0;
    PkPointF midpointRuled1;

    if (isVertical) {
        curvedBoundary0 = getSegment(Right);
        curvedBoundary1 = getSegment(Left);

        midpointRuled0 = segmentPointAt(Top, 0.5);
        midpointRuled1 = segmentPointAt(Bottom, 0.5);
    } else {
        curvedBoundary0 = getSegment(Top);
        curvedBoundary1 = getSegment(Bottom);

        midpointRuled0 = segmentPointAt(Left, 0.5);
        midpointRuled1 = segmentPointAt(Right, 0.5);
    }

    // we have to reverse it, cB1 & cB2 are in opposite direction
    std::reverse(curvedBoundary1.begin(), curvedBoundary1.end());

    // Sum of two Bezier curve is a Bezier curve
    PkVector<PkPointF> midCurved = {
        (curvedBoundary0[0] + curvedBoundary1[0]) / 2,
        (curvedBoundary0[1] + curvedBoundary1[1]) / 2,
        (curvedBoundary0[2] + curvedBoundary1[2]) / 2,
        (curvedBoundary0[3] + curvedBoundary1[3]) / 2,
    };

    // line cutting the bilinear surface in middle
    PkPointF x_2_1 = lerp(midpointRuled0, midpointRuled1, 1.0 / 3);
    PkPointF x_2_2 = lerp(midpointRuled0, midpointRuled1, 2.0 / 3);

    // line cutting rulled surface in middle
    PkPointF x_3_1 = lerp(midCurved[0], midCurved[3], 1.0 / 3);
    PkPointF x_3_2 = lerp(midCurved[0], midCurved[3], 2.0 / 3);


    p[0] = midpointRuled0;

    // X_1 = x_1_1 + x_2_1 - x_3_1
    p[1] = midCurved[1] + x_2_1 - x_3_1;

    // X_2 = x_1_2 + x_2_2 - x_3_2
    p[2] = midCurved[2] + x_2_2 - x_3_2;

    p[3] = midpointRuled1;

    return p;
}

void SvgMeshPatch::subdivideHorizontally(PkVector<SvgMeshPatch*>& subdivided,
                                         const PkVector<PkColor>& colors) const
{
    const PkPair<SvgMeshPath, SvgMeshPath> splitRight = segmentSplitAt(Right, 0.5);
    const PkPair<SvgMeshPath, SvgMeshPath> splitLeft  = segmentSplitAt(Left, 0.5);

    SvgMeshPath midHor = getMidCurve(/*isVertical = */ false);
    SvgMeshPath rMidHor = midHor;
    std::reverse(rMidHor.begin(), rMidHor.end());

    PkColor c1 = getStop(Top).color;
    PkColor c2 = getStop(Right).color;
    PkColor c3 = getStop(Bottom).color;
    PkColor c4 = getStop(Left).color;
    PkColor midc23 = colors[1];
    PkColor midc41 = colors[3];

    PkPointF midRightParametric = getMidpointParametric(Right);
    PkPointF midLeftParametric = getMidpointParametric(Left);

    SvgMeshPatch *patch = new SvgMeshPatch(getSegment(Top)[0]);
    patch->addStop(getSegment(Top), c1, Top);
    patch->addStop(splitRight.first, c2, Right);
    patch->addStop(rMidHor, midc23, Bottom);
    patch->addStop(splitLeft.second, midc41, Left);
    patch->m_parametricCoords = {
        m_parametricCoords[0],
        m_parametricCoords[1],
        midRightParametric,
        midLeftParametric
    };
    subdivided.append(patch);

    patch = new SvgMeshPatch(midHor[0]);
    patch->addStop(midHor, midc41, Top);
    patch->addStop(splitRight.second, midc23, Right);
    patch->addStop(getSegment(Bottom), c3, Bottom);
    patch->addStop(splitLeft.first,c4, Left);
    patch->m_parametricCoords = {
        midLeftParametric,
        midRightParametric,
        m_parametricCoords[2],
        m_parametricCoords[3]
    };
    subdivided.append(patch);
}

void SvgMeshPatch::subdivideVertically(PkVector<SvgMeshPatch*>& subdivided,
                                       const PkVector<PkColor>& colors) const
{
    const PkPair<SvgMeshPath, SvgMeshPath> splitTop    = segmentSplitAt(Top, 0.5);
    const PkPair<SvgMeshPath, SvgMeshPath> splitBottom = segmentSplitAt(Bottom, 0.5);

    SvgMeshPath midVer = getMidCurve(/*isVertical = */ true);
    SvgMeshPath rMidVer = midVer;
    std::reverse(rMidVer.begin(), rMidVer.end());

    PkColor c1 = getStop(Top).color;
    PkColor c2 = getStop(Right).color;
    PkColor c3 = getStop(Bottom).color;
    PkColor c4 = getStop(Left).color;
    PkColor midc12 = colors[0];
    PkColor midc34 = colors[2];

    PkPointF midTopParametric = getMidpointParametric(Top);
    PkPointF midBottomParametric = getMidpointParametric(Bottom);

    SvgMeshPatch *patch = new SvgMeshPatch(splitTop.first[0]);
    patch->addStop(splitTop.first, c1, Top);
    patch->addStop(midVer, midc12, Right);
    patch->addStop(splitBottom.second, midc34, Bottom);
    patch->addStop(getSegment(Left), c4, Left);
    patch->m_parametricCoords = {
        m_parametricCoords[0],
        midTopParametric,
        midBottomParametric,
        m_parametricCoords[3]
    };
    subdivided.append(patch);

    patch = new SvgMeshPatch(splitTop.second[0]);
    patch->addStop(splitTop.second, midc12, Top);
    patch->addStop(getSegment(Right), c2, Right);
    patch->addStop(splitBottom.first, c3, Bottom);
    patch->addStop(rMidVer, midc34, Left);
    patch->m_parametricCoords = {
        midTopParametric,
        m_parametricCoords[1],
        m_parametricCoords[2],
        midBottomParametric
    };
    subdivided.append(patch);
}

void SvgMeshPatch::subdivide(PkVector<SvgMeshPatch*>& subdivided,
                             const PkVector<PkColor>& colors) const
{
    KIS_ASSERT(colors.size() == 5);

    // The orientation is left to right and top to bottom, which means
    // Eg. the first part of splitTop is TopLeft and the second part is TopRight
    // Similarly the first part of splitRight is RightTop, but the first part of
    // splitLeft is splitLeft.second (once again, in Top to Bottom  convention)
    const PkPair<std::array<PkPointF, 4>, std::array<PkPointF, 4>> splitTop    = segmentSplitAt(Top, 0.5);
    const PkPair<std::array<PkPointF, 4>, std::array<PkPointF, 4>> splitRight  = segmentSplitAt(Right, 0.5);
    const PkPair<std::array<PkPointF, 4>, std::array<PkPointF, 4>> splitBottom = segmentSplitAt(Bottom, 0.5);
    const PkPair<std::array<PkPointF, 4>, std::array<PkPointF, 4>> splitLeft   = segmentSplitAt(Left, 0.5);

    // The way the curve and the colors at the corners are arranged before and after subdivision
    //
    //              midc12
    //       c1       +       c2
    //        +---------------+
    //        |       |       |
    //        |       | midVer|
    //        |       | <     |
    // midc41 +---------------+ midc23
    //        |  ^    |       |
    //        | midHor|       |
    //        |       |       |
    //        +---------------+
    //       c4       +       c3
    //              midc43
    //
    //             
    //  midHor --> left to right
    //  midVer --> top to bottom


    PkPair<std::array<PkPointF, 4>, std::array<PkPointF, 4>> midHor = splitAt(getMidCurve(/*isVertical = */ false), 0.5);
    PkPair<std::array<PkPointF, 4>, std::array<PkPointF, 4>> midVer = splitAt(getMidCurve(/*isVertical = */ true), 0.5);

    // middle curve is shared among the two, so we need both directions
    std::array<PkPointF, 4> reversedMidHorFirst = midHor.first;
    std::reverse(reversedMidHorFirst.begin(), reversedMidHorFirst.end());
    std::array<PkPointF, 4> reversedMidHorSecond = midHor.second;
    std::reverse(reversedMidHorSecond.begin(), reversedMidHorSecond.end());

    std::array<PkPointF, 4> reversedMidVerFirst = midVer.first;
    std::reverse(reversedMidVerFirst.begin(), reversedMidVerFirst.end());
    std::array<PkPointF, 4> reversedMidVerSecond = midVer.second;
    std::reverse(reversedMidVerSecond.begin(), reversedMidVerSecond.end());

    PkColor c1 = getStop(Top).color;
    PkColor c2 = getStop(Right).color;
    PkColor c3 = getStop(Bottom).color;
    PkColor c4 = getStop(Left).color;
    PkColor midc12 = colors[0];
    PkColor midc23 = colors[1];
    PkColor midc34 = colors[2];
    PkColor midc41 = colors[3];
    PkColor center = colors[4];

    // mid points in parametric space
    PkPointF midTopP     = getMidpointParametric(Top);
    PkPointF midRightP   = getMidpointParametric(Right);
    PkPointF midBottomP  = getMidpointParametric(Bottom);
    PkPointF midLeftP    = getMidpointParametric(Left);
    PkPointF centerP     = 0.5 * (midTopP + midBottomP);

    // patch 1: TopLeft/NorthWest
    SvgMeshPatch *patch = new SvgMeshPatch(splitTop.first[0]);
    patch->addStop(splitTop.first, c1, Top);
    patch->addStop(midVer.first, midc12, Right);
    patch->addStop(reversedMidHorFirst, center, Bottom);
    patch->addStop(splitLeft.second, midc41, Left);
    patch->m_parametricCoords = {
        m_parametricCoords[0],
        midTopP,
        centerP,
        midLeftP
    };
    subdivided.append(patch);

    // patch 2: TopRight/NorthRight
    patch = new SvgMeshPatch(splitTop.second[0]);
    patch->addStop(splitTop.second, midc12, Top);
    patch->addStop(splitRight.first, c2, Right);
    patch->addStop(reversedMidHorSecond, midc23, Bottom);
    patch->addStop(reversedMidVerFirst, center, Left);
    patch->m_parametricCoords = {
        midTopP,
        m_parametricCoords[1],
        midRightP,
        centerP
    };
    subdivided.append(patch);

    // patch 3: BottomLeft/SouthWest
    patch = new SvgMeshPatch(midHor.first[0]);
    patch->addStop(midHor.first, midc41, Top);
    patch->addStop(midVer.second, center, Right);
    patch->addStop(splitBottom.second, midc34, Bottom);
    patch->addStop(splitLeft.first, c4, Left);
    patch->m_parametricCoords = {
        midLeftP,
        centerP,
        midBottomP,
        m_parametricCoords[3]
    };
    subdivided.append(patch);

    // patch 4: BottomRight/SouthEast
    patch = new SvgMeshPatch(midHor.second[0]);
    patch->addStop(midHor.second, center, Top);
    patch->addStop(splitRight.second, midc23, Right);
    patch->addStop(splitBottom.first, c3, Bottom);
    patch->addStop(reversedMidVerSecond, midc34, Left);
    patch->m_parametricCoords = {
        centerP,
        midRightP,
        m_parametricCoords[2],
        midBottomP
    };
    subdivided.append(patch);
}

static qreal controlrectLen(const SvgMeshPath &path) {
    return PkLineF(path[0], path[1]).length() +
        PkLineF(path[1], path[2]).length() +
        PkLineF(path[2], path[3]).length();
}

bool SvgMeshPatch::isDivisibleVertically() const
{
    // I arrived at this number by the virtue called trial 'n error
    const qreal minlength = 1.7;
    const qreal line1 = PkLineF(controlPoints[Top][0], controlPoints[Top][3]).length();
    const qreal control1 = controlrectLen(getSegment(Top));

    // a decent average, thanks to Khronos's forums
    if ((line1 + control1 / 2) < minlength) {
        return false;
    }

    const qreal line2 = PkLineF(controlPoints[Bottom][0], controlPoints[Bottom][3]).length();
    const qreal control2 = controlrectLen(getSegment(Bottom));
    if ((line2 + control2 / 2) < minlength) {
        return false;
    }

    return true;
}

bool SvgMeshPatch::isDivisibleHorizontally() const
{
    // I arrived at this number by the virtue called trial 'n error
    const qreal minlength = 1.7;

    // a decent average, thanks to Khronos's forums
    const qreal line1 = PkLineF(controlPoints[Right][0], controlPoints[Right][3]).length();
    const qreal control1 = controlrectLen(getSegment(Right));
    if ((line1 + control1 / 2) < minlength) {
        return false;
    }

    const qreal line2 = PkLineF(controlPoints[Left][0], controlPoints[Left][3]).length();
    const qreal control2 = controlrectLen(getSegment(Left));
    if ((line2 + control2 / 2) < minlength) {
        return false;
    }

    return true;
}

void SvgMeshPatch::addStop(const PkString& pathStr,
                           PkColor color,
                           Type edge,
                           bool pathIncomplete,
                           PkPointF lastPoint)
{
    SvgMeshStop node(color, m_startingPoint);
    m_nodes[edge] = node;

    m_startingPoint = parseMeshPath(pathStr, pathIncomplete, lastPoint);
}

void SvgMeshPatch::addStop(const std::array<PkPointF, 4>& pathPoints, PkColor color, Type edge)
{
    SvgMeshStop stop(color, pathPoints[0]);
    m_nodes[edge] = stop;

    if (edge == SvgMeshPatch::Top) {
        moveTo(pathPoints[0]);
        m_newPath = false;
    }

    curveTo(pathPoints[1], pathPoints[2], pathPoints[3]);
    m_startingPoint = pathPoints[3];
}


void SvgMeshPatch::addStopLinear(const std::array<PkPointF, 2>& pathPoints, PkColor color, Type edge)
{
    SvgMeshStop stop(color, pathPoints[0]);
    m_nodes[edge] = stop;

    if (edge == SvgMeshPatch::Top) {
        moveTo(pathPoints[0]);
        m_newPath = false;
    }

    lineTo(pathPoints[1]);
    m_startingPoint = pathPoints[1];
}

void SvgMeshPatch::modifyPath(SvgMeshPatch::Type type, std::array<PkPointF, 4> newPath)
{
    controlPoints[type] = newPath;
    m_nodes[type].point = newPath[0];
}

void SvgMeshPatch::modifyCorner(SvgMeshPatch::Type type, const PkPointF &delta)
{
    controlPoints[type][0] -= delta;
    controlPoints[type][1] -= delta;
    m_nodes[type].point = controlPoints[type][0];

    controlPoints[(Size + type - 1) % Size][3] -= delta;
    controlPoints[(Size + type - 1) % Size][2] -= delta;
}

void SvgMeshPatch::setStopColor(SvgMeshPatch::Type type, const PkColor &color)
{
    m_nodes[type].color = color;
}

void SvgMeshPatch::setTransform(const PkTransform& matrix)
{
    m_startingPoint = matrix.map(m_startingPoint);
    for (int i = 0; i < Size; ++i) {
        m_nodes[i].point = matrix.map(m_nodes[i].point);
        for (int j = 0; j < 4; ++j) {
            controlPoints[i][j] = matrix.map(controlPoints[i][j]);
        }
    }
}

int SvgMeshPatch::countPoints() const
{
    return m_nodes.size();
}


PkPointF SvgMeshPatch::parseMeshPath(const PkString& s, bool pathIncomplete, const PkPointF lastPoint)
{
    // bits and pieces from KoPathShapeLoader, see the copyright above
    if (!s.isEmpty()) {
        PkString d = s;
        d = pkSimplified(pkReplaceChar(d, ',', ' '));

        const std::string buffer = d.PkToUtf8();
        const char *ptr = buffer.c_str();
        qreal curx = m_startingPoint.x();
        qreal cury = m_startingPoint.y();
        qreal tox, toy, x1, y1, x2, y2;
        bool relative = false;
        char command = *(ptr++);

        if (m_newPath) {
            moveTo(m_startingPoint);
            m_newPath = false;
        }

       while (*ptr == ' ')
           ++ptr;

       switch (command) {
       case 'l':
           relative = true;
           [[fallthrough]];
       case 'L': {
           ptr = getCoord(ptr, tox);
           ptr = getCoord(ptr, toy);

           if (relative) {
               tox = curx + tox;
               toy = cury + toy;
           }

           if (pathIncomplete) {
               tox = lastPoint.x();
               toy = lastPoint.y();
           }

           // we convert lines to cubic curve
           lineTo({tox, toy});
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

           if (relative) {
               x1  = curx + x1;
               y1  = cury + y1;
               x2  = curx + x2;
               y2  = cury + y2;
               tox = curx + tox;
               toy = cury + toy;
           }

           if (pathIncomplete) {
               tox = lastPoint.x();
               toy = lastPoint.y();
           }

           curveTo(PkPointF(x1, y1), PkPointF(x2, y2), PkPointF(tox, toy));
           break;
       }

       default: {
           qWarning() << "SvgMeshPatch::parseMeshPath: Bad command \"" << command << "\"";
           return PkPointF();
       }
       }
       return {tox, toy};
    }
    return PkPointF();
}

const char* SvgMeshPatch::getCoord(const char* ptr, qreal& number)
{
    // copied from KoPathShapeLoader, see the copyright above
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
            exponent *= 10;
            exponent += *ptr - '0';
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
