/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is derived from the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 2.0 or (at your option) the GNU General Public
** license version 3 or any later version approved by the KDE Free Qt
** Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "kis_path_stroker_p.h"

#include <PkLine.h>
#include <PkPainterPath.h>
#include <PkPen.h>
#include <PkRect.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <tuple>
#include <utility>
#include <vector>

namespace KisPathRasterizer::Private {
namespace {

using Fixed = qreal;

constexpr qreal pathKappa = 0.5522847498;

bool fuzzyCompare(qreal a, qreal b)
{
    return pkQtFuzzyCompare(a, b);
}

bool fuzzyIsNull(qreal value)
{
    return pkQtFuzzyIsNull(value);
}

struct FixedPoint {
    Fixed x = 0;
    Fixed y = 0;

    bool isFinite() const { return std::isfinite(x) && std::isfinite(y); }
    bool operator==(const FixedPoint &other) const
    {
        return fuzzyCompare(x, other.x) && fuzzyCompare(y, other.y);
    }
};

struct RectD {
    qreal x = 0;
    qreal y = 0;
    qreal width = 0;
    qreal height = 0;

    bool isEmpty() const { return width <= 0 || height <= 0; }
    bool isNull() const { return width == 0 && height == 0; }
    qreal left() const { return x; }
    qreal right() const { return x + width; }
    qreal top() const { return y; }
    qreal bottom() const { return y + height; }
    PkPointF center() const { return PkPointF(x + width / 2, y + height / 2); }
};

struct Bezier {
    qreal x1 = 0;
    qreal y1 = 0;
    qreal x2 = 0;
    qreal y2 = 0;
    qreal x3 = 0;
    qreal y3 = 0;
    qreal x4 = 0;
    qreal y4 = 0;

    static Bezier fromPoints(const PkPointF &p1, const PkPointF &p2,
                             const PkPointF &p3, const PkPointF &p4)
    {
        return {p1.x(), p1.y(), p2.x(), p2.y(),
                p3.x(), p3.y(), p4.x(), p4.y()};
    }

    PkPointF pt1() const { return PkPointF(x1, y1); }
    PkPointF pt2() const { return PkPointF(x2, y2); }
    PkPointF pt3() const { return PkPointF(x3, y3); }
    PkPointF pt4() const { return PkPointF(x4, y4); }

    PkPointF pointAt(qreal t) const
    {
        const qreal mt = 1.0 - t;
        qreal a = x1 * mt + x2 * t;
        qreal b = x2 * mt + x3 * t;
        qreal c = x3 * mt + x4 * t;
        a = a * mt + b * t;
        b = b * mt + c * t;
        const qreal x = a * mt + b * t;

        a = y1 * mt + y2 * t;
        b = y2 * mt + y3 * t;
        c = y3 * mt + y4 * t;
        a = a * mt + b * t;
        b = b * mt + c * t;
        return PkPointF(x, a * mt + b * t);
    }

    PkPointF normalVector(qreal t) const
    {
        const qreal mt = 1.0 - t;
        const qreal a = mt * mt;
        const qreal b = t * mt;
        const qreal c = t * t;
        return PkPointF((y2 - y1) * a + (y3 - y2) * b + (y4 - y3) * c,
                        -(x2 - x1) * a - (x3 - x2) * b - (x4 - x3) * c);
    }

    RectD bounds() const
    {
        const qreal xmin = std::min({x1, x2, x3, x4});
        const qreal xmax = std::max({x1, x2, x3, x4});
        const qreal ymin = std::min({y1, y2, y3, y4});
        const qreal ymax = std::max({y1, y2, y3, y4});
        return {xmin, ymin, xmax - xmin, ymax - ymin};
    }

    PkLineF startTangent() const
    {
        PkLineF tangent(pt1(), pt2());
        if (tangent.isNull()) tangent = PkLineF(pt1(), pt3());
        if (tangent.isNull()) tangent = PkLineF(pt1(), pt4());
        return tangent;
    }

    std::pair<Bezier, Bezier> split() const
    {
        const auto mid = [](const PkPointF &lhs, const PkPointF &rhs) {
            return (lhs + rhs) * 0.5;
        };
        const PkPointF mid12 = mid(pt1(), pt2());
        const PkPointF mid23 = mid(pt2(), pt3());
        const PkPointF mid34 = mid(pt3(), pt4());
        const PkPointF mid1223 = mid(mid12, mid23);
        const PkPointF mid2334 = mid(mid23, mid34);
        const PkPointF center = mid(mid1223, mid2334);
        return {fromPoints(pt1(), mid12, mid1223, center),
                fromPoints(center, mid2334, mid34, pt4())};
    }

    void addToPolygon(std::vector<PkPointF> &polygon, qreal threshold) const
    {
        std::array<Bezier, 10> beziers;
        std::array<int, 10> levels;
        beziers[0] = *this;
        levels[0] = 9;
        int top = 0;
        while (top >= 0) {
            Bezier *b = &beziers[std::size_t(top)];
            const qreal y4y1 = b->y4 - b->y1;
            const qreal x4x1 = b->x4 - b->x1;
            qreal length = std::abs(x4x1) + std::abs(y4y1);
            qreal distance;
            if (length > 1.0) {
                distance = std::abs(x4x1 * (b->y1 - b->y2)
                                    - y4y1 * (b->x1 - b->x2))
                    + std::abs(x4x1 * (b->y1 - b->y3)
                               - y4y1 * (b->x1 - b->x3));
            } else {
                distance = std::abs(b->x1 - b->x2) + std::abs(b->y1 - b->y2)
                    + std::abs(b->x1 - b->x3) + std::abs(b->y1 - b->y3);
                length = 1.0;
            }
            if (distance < threshold * length || levels[std::size_t(top)] == 0) {
                polygon.push_back(b->pt4());
                --top;
            } else {
                std::tie(b[1], b[0]) = b->split();
                levels[std::size_t(top + 1)] = --levels[std::size_t(top)];
                ++top;
            }
        }
    }

    int shifted(Bezier *segments, int maxSegments, qreal offset, qreal threshold) const;
};

enum class ShiftResult { Ok, Discard, Split, Circle };

ShiftResult goodOffset(const Bezier &source, const Bezier &shifted,
                       qreal offset, qreal threshold)
{
    const qreal offsetSquared = offset * offset;
    const qreal maxLineDistance = threshold * offset * offset;
    const qreal maxNormalDistance = threshold * offset;
    for (qreal t = 0.25; t < 0.99; t += 0.25) {
        const PkPointF p1 = source.pointAt(t);
        const PkPointF p2 = shifted.pointAt(t);
        qreal distance = (p1.x() - p2.x()) * (p1.x() - p2.x())
            + (p1.y() - p2.y()) * (p1.y() - p2.y());
        if (std::abs(distance - offsetSquared) > maxLineDistance) return ShiftResult::Split;
        const PkPointF normal = source.normalVector(t);
        const qreal length = std::abs(normal.x()) + std::abs(normal.y());
        if (length != 0.0) {
            distance = std::abs(normal.x() * (p1.y() - p2.y())
                                - normal.y() * (p1.x() - p2.x())) / length;
            if (distance > maxNormalDistance) return ShiftResult::Split;
        }
    }
    return ShiftResult::Ok;
}

ShiftResult shiftBezier(const Bezier &source, Bezier &shifted,
                        qreal offset, qreal threshold)
{
    int map[4];
    const bool p12Equal = fuzzyCompare(source.x1, source.x2)
        && fuzzyCompare(source.y1, source.y2);
    const bool p23Equal = fuzzyCompare(source.x2, source.x3)
        && fuzzyCompare(source.y2, source.y3);
    const bool p34Equal = fuzzyCompare(source.x3, source.x4)
        && fuzzyCompare(source.y3, source.y4);
    PkPointF points[4];
    int count = 0;
    points[count] = source.pt1();
    map[0] = 0;
    ++count;
    if (!p12Equal) points[count++] = source.pt2();
    map[1] = count - 1;
    if (!p23Equal) points[count++] = source.pt3();
    map[2] = count - 1;
    if (!p34Equal) points[count++] = source.pt4();
    map[3] = count - 1;
    if (count == 1) return ShiftResult::Discard;

    const RectD bounds = source.bounds();
    if (count == 4 && bounds.width < 0.1 * offset && bounds.height < 0.1 * offset) {
        const qreal length = (source.x1 - source.x2) * (source.x1 - source.x2)
            + (source.y1 - source.y2) * (source.y1 - source.y2)
                * (source.x3 - source.x4) * (source.x3 - source.x4)
            + (source.y3 - source.y4) * (source.y3 - source.y4);
        const qreal dot = (source.x1 - source.x2) * (source.x3 - source.x4)
            + (source.y1 - source.y2) * (source.y3 - source.y4);
        if (dot < 0 && dot * dot < 0.8 * length) return ShiftResult::Circle;
    }

    PkPointF shiftedPoints[4];
    PkLineF previous(PkPointF(), points[1] - points[0]);
    if (!previous.length()) return ShiftResult::Discard;
    PkPointF previousNormal = previous.normalVector().unitVector().p2();
    shiftedPoints[0] = points[0] + offset * previousNormal;
    for (int i = 1; i < count - 1; ++i) {
        const PkLineF next(PkPointF(), points[i + 1] - points[i]);
        const PkPointF nextNormal = next.normalVector().unitVector().p2();
        const PkPointF normalSum = previousNormal + nextNormal;
        const qreal denominator = 1.0 + previousNormal.x() * nextNormal.x()
            + previousNormal.y() * nextNormal.y();
        if (fuzzyIsNull(denominator)) {
            shiftedPoints[i] = points[i] + offset * previousNormal;
        } else {
            shiftedPoints[i] = points[i] + (offset / denominator) * normalSum;
        }
        previousNormal = nextNormal;
    }
    shiftedPoints[count - 1] = points[count - 1] + offset * previousNormal;
    shifted = Bezier::fromPoints(shiftedPoints[map[0]], shiftedPoints[map[1]],
                                 shiftedPoints[map[2]], shiftedPoints[map[3]]);
    return count > 2 ? goodOffset(source, shifted, offset, threshold) : ShiftResult::Ok;
}

bool addCircle(const Bezier &source, qreal offset, Bezier *output)
{
    PkPointF normals[3];
    normals[0] = PkPointF(source.y2 - source.y1, source.x1 - source.x2);
    qreal distance = std::hypot(normals[0].x(), normals[0].y());
    if (fuzzyIsNull(distance)) return false;
    normals[0] /= distance;
    normals[2] = PkPointF(source.y4 - source.y3, source.x3 - source.x4);
    distance = std::hypot(normals[2].x(), normals[2].y());
    if (fuzzyIsNull(distance)) return false;
    normals[2] /= distance;
    normals[1] = PkPointF(source.x1 - source.x2 - source.x3 + source.x4,
                          source.y1 - source.y2 - source.y3 + source.y4);
    normals[1] /= -std::hypot(normals[1].x(), normals[1].y());

    qreal angles[2];
    qreal sign = 1.0;
    for (int i = 0; i < 2; ++i) {
        qreal cosine = normals[i].x() * normals[i + 1].x()
            + normals[i].y() * normals[i + 1].y();
        cosine = std::clamp(cosine, qreal(-1), qreal(1));
        angles[i] = std::acos(cosine) / M_PI;
    }
    if (angles[0] + angles[1] > 1.0) {
        normals[1] = -normals[1];
        angles[0] = 1.0 - angles[0];
        angles[1] = 1.0 - angles[1];
        sign = -1.0;
    }
    const PkPointF circle[3] = {
        source.pt1() + normals[0] * offset,
        PkPointF(0.5 * (source.x1 + source.x4), 0.5 * (source.y1 + source.y4))
            + normals[1] * offset,
        source.pt4() + normals[2] * offset
    };
    for (int i = 0; i < 2; ++i) {
        const qreal kappa = 2.0 * pathKappa * sign * offset * angles[i];
        output[i] = Bezier::fromPoints(
            circle[i],
            PkPointF(circle[i].x() - normals[i].y() * kappa,
                     circle[i].y() + normals[i].x() * kappa),
            PkPointF(circle[i + 1].x() + normals[i + 1].y() * kappa,
                     circle[i + 1].y() - normals[i + 1].x() * kappa),
            circle[i + 1]);
    }
    return true;
}

int Bezier::shifted(Bezier *segments, int maxSegments,
                    qreal offset, qreal threshold) const
{
    if (fuzzyCompare(x1, x2) && fuzzyCompare(x1, x3) && fuzzyCompare(x1, x4)
        && fuzzyCompare(y1, y2) && fuzzyCompare(y1, y3) && fuzzyCompare(y1, y4)) {
        return 0;
    }
    --maxSegments;
    std::array<Bezier, 10> beziers;
redo:
    beziers[0] = *this;
    Bezier *current = beziers.data();
    Bezier *output = segments;
    while (current >= beziers.data()) {
        const int stackSegments = int(current - beziers.data()) + 1;
        if (stackSegments == 10 || output - segments == maxSegments - stackSegments) {
            threshold *= 1.5;
            if (threshold > 2.0) goto giveUp;
            goto redo;
        }
        const ShiftResult result = shiftBezier(*current, *output, offset, threshold);
        if (result == ShiftResult::Discard) {
            --current;
        } else if (result == ShiftResult::Ok) {
            ++output;
            --current;
        } else if (result == ShiftResult::Circle
                   && maxSegments - (output - segments) >= 2) {
            if (addCircle(*current, offset, output)) output += 2;
            --current;
        } else {
            std::tie(current[1], current[0]) = current->split();
            ++current;
        }
    }
giveUp:
    while (current >= beziers.data()) {
        const ShiftResult result = shiftBezier(*current, *output, offset, threshold);
        if (result == ShiftResult::Ok || result == ShiftResult::Split) ++output;
        --current;
    }
    return int(output - segments);
}

using MoveHook = void (*)(Fixed, Fixed, void *);
using LineHook = void (*)(Fixed, Fixed, void *);
using CubicHook = void (*)(Fixed, Fixed, Fixed, Fixed, Fixed, Fixed, void *);

class StrokerOps
{
public:
    struct Element {
        PkPainterPath::ElementType type;
        Fixed x;
        Fixed y;
        bool isMoveTo() const { return type == PkPainterPath::MoveToElement; }
        bool isLineTo() const { return type == PkPainterPath::LineToElement; }
        bool isCurveTo() const { return type == PkPainterPath::CurveToElement; }
        operator FixedPoint() const { return {x, y}; }
    };

    virtual ~StrokerOps() = default;
    void setHooks(MoveHook move, LineHook line, CubicHook cubic)
    {
        m_moveHook = move;
        m_lineHook = line;
        m_cubicHook = cubic;
    }
    virtual void begin(void *data) { m_customData = data; m_elements.clear(); }
    virtual void end()
    {
        if (m_elements.size() > 1) processCurrentSubpath();
        m_customData = nullptr;
    }
    void moveTo(Fixed x, Fixed y)
    {
        if (m_elements.size() > 1) processCurrentSubpath();
        m_elements.clear();
        m_elements.push_back({PkPainterPath::MoveToElement, x, y});
    }
    void lineTo(Fixed x, Fixed y)
    {
        m_elements.push_back({PkPainterPath::LineToElement, x, y});
    }
    void cubicTo(Fixed x1, Fixed y1, Fixed x2, Fixed y2, Fixed x3, Fixed y3)
    {
        m_elements.push_back({PkPainterPath::CurveToElement, x1, y1});
        m_elements.push_back({PkPainterPath::CurveToDataElement, x2, y2});
        m_elements.push_back({PkPainterPath::CurveToDataElement, x3, y3});
    }
    void strokePath(const PkPainterPath &path, void *data)
    {
        if (path.isEmpty()) return;
        m_dashThreshold = 0.5;
        begin(data);
        for (int i = 0; i < path.elementCount(); ++i) {
            const auto element = path.elementAt(i);
            switch (element.type) {
            case PkPainterPath::MoveToElement:
                moveTo(element.x, element.y);
                break;
            case PkPainterPath::LineToElement:
                lineTo(element.x, element.y);
                break;
            case PkPainterPath::CurveToElement: {
                const auto control2 = path.elementAt(++i);
                const auto endPoint = path.elementAt(++i);
                cubicTo(element.x, element.y, control2.x, control2.y,
                        endPoint.x, endPoint.y);
                break;
            }
            default:
                break;
            }
        }
        end();
    }
    void setClipRect(const RectD &clip) { m_clipRect = clip; }
    const RectD &clipRect() const { return m_clipRect; }
    void setCurveThreshold(Fixed threshold) { m_curveThreshold = threshold; }
    Fixed curveThreshold() const { return m_curveThreshold; }

protected:
    void emitMoveTo(Fixed x, Fixed y) { m_moveHook(x, y, m_customData); }
    void emitLineTo(Fixed x, Fixed y) { m_lineHook(x, y, m_customData); }
    void emitCubicTo(Fixed x1, Fixed y1, Fixed x2, Fixed y2, Fixed x3, Fixed y3)
    {
        m_cubicHook(x1, y1, x2, y2, x3, y3, m_customData);
    }
    virtual void processCurrentSubpath() = 0;

    std::vector<Element> m_elements;
    RectD m_clipRect;
    Fixed m_curveThreshold = 0.25;
    Fixed m_dashThreshold = 0.25;
    void *m_customData = nullptr;
    MoveHook m_moveHook = nullptr;
    LineHook m_lineHook = nullptr;
    CubicHook m_cubicHook = nullptr;
};

class ForwardIterator
{
public:
    explicit ForwardIterator(const std::vector<StrokerOps::Element> &path) : m_path(path) {}
    bool hasNext() const { return m_position < int(m_path.size()); }
    StrokerOps::Element next() { return m_path.at(std::size_t(m_position++)); }
private:
    const std::vector<StrokerOps::Element> &m_path;
    int m_position = 0;
};

class BackwardIterator
{
public:
    explicit BackwardIterator(const std::vector<StrokerOps::Element> &path)
        : m_path(path), m_position(int(path.size()) - 1) {}
    bool hasNext() const { return m_position >= 0; }
    StrokerOps::Element next()
    {
        StrokerOps::Element current = m_path.at(std::size_t(m_position));
        if (m_position == int(m_path.size()) - 1) {
            --m_position;
            current.type = PkPainterPath::MoveToElement;
            return current;
        }
        const auto &previous = m_path.at(std::size_t(m_position + 1));
        switch (previous.type) {
        case PkPainterPath::LineToElement:
            current.type = PkPainterPath::LineToElement;
            break;
        case PkPainterPath::CurveToDataElement:
            current.type = current.type == PkPainterPath::CurveToElement
                ? PkPainterPath::CurveToDataElement : PkPainterPath::CurveToElement;
            break;
        case PkPainterPath::CurveToElement:
            current.type = PkPainterPath::CurveToDataElement;
            break;
        default:
            break;
        }
        --m_position;
        return current;
    }
private:
    const std::vector<StrokerOps::Element> &m_path;
    int m_position;
};

class FlatIterator
{
public:
    FlatIterator(const std::vector<StrokerOps::Element> &path, qreal threshold)
        : m_path(path), m_threshold(threshold) {}
    bool hasNext() const { return m_curveIndex >= 0 || m_position < int(m_path.size()); }
    StrokerOps::Element next()
    {
        if (m_curveIndex >= 0) {
            const auto &point = m_curve.at(std::size_t(m_curveIndex++));
            if (m_curveIndex >= int(m_curve.size())) m_curveIndex = -1;
            return {PkPainterPath::LineToElement, point.x(), point.y()};
        }
        StrokerOps::Element element = m_path.at(std::size_t(m_position));
        if (element.isCurveTo()) {
            const auto &start = m_path.at(std::size_t(m_position - 1));
            const auto &control2 = m_path.at(std::size_t(m_position + 1));
            const auto &end = m_path.at(std::size_t(m_position + 2));
            const Bezier bezier = Bezier::fromPoints(
                PkPointF(start.x, start.y), PkPointF(element.x, element.y),
                PkPointF(control2.x, control2.y), PkPointF(end.x, end.y));
            m_curve.clear();
            m_curve.push_back(bezier.pt1());
            bezier.addToPolygon(m_curve, m_threshold);
            m_curveIndex = 1;
            element.type = PkPainterPath::LineToElement;
            element.x = m_curve.front().x();
            element.y = m_curve.front().y();
            m_position += 2;
        }
        ++m_position;
        return element;
    }
private:
    const std::vector<StrokerOps::Element> &m_path;
    int m_position = 0;
    std::vector<PkPointF> m_curve;
    int m_curveIndex = -1;
    qreal m_threshold;
};

qreal angleOnX(const PkLineF &line)
{
    return PkLineF(0, 0, 1, 0).angleTo(line);
}

void bezierCoefficients(qreal t, qreal &a, qreal &b, qreal &c, qreal &d)
{
    const qreal mt = 1.0 - t;
    b = mt * mt;
    c = t * t;
    d = c * t;
    a = b * mt;
    b *= 3.0 * t;
    c *= 3.0 * mt;
}

qreal tForArcAngle(qreal angle)
{
    if (fuzzyIsNull(angle)) return 0;
    if (fuzzyCompare(angle, qreal(90))) return 1;
    const qreal radians = angle * M_PI / 180.0;
    const qreal cosine = std::cos(radians);
    const qreal sine = std::sin(radians);
    qreal tc = angle / 90.0;
    for (int i = 0; i < 2; ++i) {
        tc -= ((((2 - 3 * pathKappa) * tc + 3 * (pathKappa - 1)) * tc) * tc
               + 1 - cosine)
            / (((6 - 9 * pathKappa) * tc + 6 * (pathKappa - 1)) * tc);
    }
    qreal ts = tc;
    for (int i = 0; i < 2; ++i) {
        ts -= ((((3 * pathKappa - 2) * ts - 6 * pathKappa + 3) * ts
                + 3 * pathKappa) * ts - sine)
            / (((9 * pathKappa - 6) * ts + 12 * pathKappa - 6) * ts
               + 3 * pathKappa);
    }
    return 0.5 * (tc + ts);
}

void findEllipseCoords(const RectD &rect, qreal angle, qreal length,
                       PkPointF *startPoint, PkPointF *endPoint)
{
    if (rect.isNull()) {
        if (startPoint) *startPoint = PkPointF();
        if (endPoint) *endPoint = PkPointF();
        return;
    }
    const qreal halfWidth = rect.width / 2;
    const qreal halfHeight = rect.height / 2;
    const qreal angles[2] = {angle, angle + length};
    PkPointF *points[2] = {startPoint, endPoint};
    for (int i = 0; i < 2; ++i) {
        if (!points[i]) continue;
        const qreal theta = angles[i] - 360 * std::floor(angles[i] / 360);
        qreal t = theta / 90;
        const int quadrant = int(t);
        t -= quadrant;
        t = tForArcAngle(90 * t);
        if (quadrant & 1) t = 1 - t;
        qreal a, b, c, d;
        bezierCoefficients(t, a, b, c, d);
        PkPointF point(a + b + c * pathKappa, d + c + b * pathKappa);
        if (quadrant == 1 || quadrant == 2) point.setX(-point.x());
        if (quadrant == 0 || quadrant == 1) point.setY(-point.y());
        *points[i] = rect.center()
            + PkPointF(halfWidth * point.x(), halfHeight * point.y());
    }
}

PkPointF curvesForArc(const RectD &rect, qreal startAngle, qreal sweepLength,
                      PkPointF *curves, int *pointCount)
{
    *pointCount = 0;
    if (rect.isNull()) return PkPointF();
    const qreal halfWidth = rect.width / 2;
    const qreal halfHeight = rect.height / 2;
    const qreal widthKappa = halfWidth * pathKappa;
    const qreal heightKappa = halfHeight * pathKappa;
    const qreal x = rect.x;
    const qreal y = rect.y;
    const qreal width = rect.width;
    const qreal height = rect.height;
    const PkPointF points[13] = {
        {x + width, y + halfHeight},
        {x + width, y + halfHeight + heightKappa},
        {x + halfWidth + widthKappa, y + height},
        {x + halfWidth, y + height},
        {x + halfWidth - widthKappa, y + height},
        {x, y + halfHeight + heightKappa},
        {x, y + halfHeight},
        {x, y + halfHeight - heightKappa},
        {x + halfWidth - widthKappa, y},
        {x + halfWidth, y},
        {x + halfWidth + widthKappa, y},
        {x + width, y + halfHeight - heightKappa},
        {x + width, y + halfHeight}
    };
    sweepLength = std::clamp(sweepLength, qreal(-360), qreal(360));
    if (startAngle == 0.0 && sweepLength == 360.0) {
        for (int i = 11; i >= 0; --i) curves[(*pointCount)++] = points[i];
        return points[12];
    }
    if (startAngle == 0.0 && sweepLength == -360.0) {
        for (int i = 1; i <= 12; ++i) curves[(*pointCount)++] = points[i];
        return points[0];
    }
    int startSegment = int(std::floor(startAngle / 90));
    int endSegment = int(std::floor((startAngle + sweepLength) / 90));
    qreal startT = (startAngle - startSegment * 90) / 90;
    qreal endT = (startAngle + sweepLength - endSegment * 90) / 90;
    const int delta = sweepLength > 0 ? 1 : -1;
    if (delta < 0) { startT = 1 - startT; endT = 1 - endT; }
    if (fuzzyIsNull(startT - 1.0)) { startT = 0; startSegment += delta; }
    if (fuzzyIsNull(endT)) { endT = 1; endSegment -= delta; }
    startT = tForArcAngle(startT * 90);
    endT = tForArcAngle(endT * 90);
    const bool splitAtStart = !fuzzyIsNull(startT);
    const bool splitAtEnd = !fuzzyIsNull(endT - 1.0);
    const int end = endSegment + delta;
    if (startSegment == end) {
        const int quadrant = 3 - ((startSegment % 4) + 4) % 4;
        const int index = 3 * quadrant;
        return delta > 0 ? points[index + 3] : points[index];
    }
    PkPointF startPoint;
    PkPointF endPoint;
    findEllipseCoords(rect, startAngle, sweepLength, &startPoint, &endPoint);
    for (int i = startSegment; i != end; i += delta) {
        const int quadrant = 3 - ((i % 4) + 4) % 4;
        const int index = 3 * quadrant;
        Bezier bezier = delta > 0
            ? Bezier::fromPoints(points[index + 3], points[index + 2],
                                 points[index + 1], points[index])
            : Bezier::fromPoints(points[index], points[index + 1],
                                 points[index + 2], points[index + 3]);
        if (startSegment == endSegment && fuzzyCompare(startT, endT)) return startPoint;
        // Round joins only request a full quadrant boundary or a single partial
        // segment. Port the exact interval split used by Qt for the latter.
        auto interval = [](Bezier source, qreal t0, qreal t1) {
            auto splitLeft = [](Bezier &sourceBezier, qreal t, Bezier &left) {
                left.x1 = sourceBezier.x1; left.y1 = sourceBezier.y1;
                left.x2 = sourceBezier.x1 + t * (sourceBezier.x2 - sourceBezier.x1);
                left.y2 = sourceBezier.y1 + t * (sourceBezier.y2 - sourceBezier.y1);
                left.x3 = sourceBezier.x2 + t * (sourceBezier.x3 - sourceBezier.x2);
                left.y3 = sourceBezier.y2 + t * (sourceBezier.y3 - sourceBezier.y2);
                sourceBezier.x3 += t * (sourceBezier.x4 - sourceBezier.x3);
                sourceBezier.y3 += t * (sourceBezier.y4 - sourceBezier.y3);
                sourceBezier.x2 = left.x3 + t * (sourceBezier.x3 - left.x3);
                sourceBezier.y2 = left.y3 + t * (sourceBezier.y3 - left.y3);
                left.x3 = left.x2 + t * (left.x3 - left.x2);
                left.y3 = left.y2 + t * (left.y3 - left.y2);
                left.x4 = sourceBezier.x1 = left.x3 + t * (sourceBezier.x2 - left.x3);
                left.y4 = sourceBezier.y1 = left.y3 + t * (sourceBezier.y2 - left.y3);
            };
            Bezier result;
            Bezier temporary;
            if (fuzzyIsNull(t1 - 1.0)) result = source;
            else { temporary = source; splitLeft(temporary, t1, result); }
            if (!fuzzyIsNull(t0)) splitLeft(result, t0 / t1, temporary);
            return result;
        };
        if (i == startSegment) {
            if (i == endSegment && splitAtEnd) bezier = interval(bezier, startT, endT);
            else if (splitAtStart) bezier = interval(bezier, startT, 1);
        } else if (i == endSegment && splitAtEnd) {
            bezier = interval(bezier, 0, endT);
        }
        curves[(*pointCount)++] = bezier.pt2();
        curves[(*pointCount)++] = bezier.pt3();
        curves[(*pointCount)++] = bezier.pt4();
    }
    curves[*pointCount - 1] = endPoint;
    return startPoint;
}

class Stroker;

template <class Iterator>
bool strokeSide(Iterator *iterator, Stroker *stroker,
                bool capFirst, PkLineF *startTangent);

class Stroker : public StrokerOps
{
public:
    enum class JoinMode { Flat, Square, Miter, Round, RoundCap, SvgMiter };

    void setStrokeWidth(Fixed width)
    {
        m_strokeWidth = width;
        m_curveThreshold = std::clamp(1.0 / width, qreal(0.00025), qreal(0.25));
    }
    Fixed strokeWidth() const { return m_strokeWidth; }
    void setCapStyle(Qt::PenCapStyle style)
    {
        m_capStyle = style == Qt::FlatCap ? JoinMode::Flat
            : style == Qt::SquareCap ? JoinMode::Square : JoinMode::RoundCap;
    }
    void setJoinStyle(Qt::PenJoinStyle style)
    {
        m_joinStyle = style == Qt::BevelJoin ? JoinMode::Flat
            : style == Qt::MiterJoin ? JoinMode::Miter
            : style == Qt::SvgMiterJoin ? JoinMode::SvgMiter : JoinMode::Round;
    }
    void setMiterLimit(Fixed limit) { m_miterLimit = limit; }
    Fixed miterLimit() const { return m_miterLimit; }
    JoinMode capStyleMode() const { return m_capStyle; }
    JoinMode joinStyleMode() const { return m_joinStyle; }

    void outputMoveTo(Fixed x, Fixed y)
    {
        m_back2X = m_back1X; m_back2Y = m_back1Y;
        m_back1X = x; m_back1Y = y;
        StrokerOps::emitMoveTo(x, y);
    }
    void outputLineTo(Fixed x, Fixed y)
    {
        m_back2X = m_back1X; m_back2Y = m_back1Y;
        m_back1X = x; m_back1Y = y;
        StrokerOps::emitLineTo(x, y);
    }
    void outputCubicTo(Fixed c1x, Fixed c1y, Fixed c2x, Fixed c2y,
                       Fixed endX, Fixed endY)
    {
        if (c2x == endX && c2y == endY) {
            if (c1x == endX && c1y == endY) {
                m_back2X = m_back1X; m_back2Y = m_back1Y;
            } else {
                m_back2X = c1x; m_back2Y = c1y;
            }
        } else {
            m_back2X = c2x; m_back2Y = c2y;
        }
        m_back1X = endX; m_back1Y = endY;
        StrokerOps::emitCubicTo(c1x, c1y, c2x, c2y, endX, endY);
    }

    void joinPoints(Fixed focalX, Fixed focalY, const PkLineF &nextLine, JoinMode join)
    {
        if (fuzzyCompare(m_back1X, nextLine.x1())
            && fuzzyCompare(m_back1Y, nextLine.y1())) return;
        const PkLineF previousLine(m_back2X, m_back2Y, m_back1X, m_back1Y);
        PkPointF intersection;
        const auto type = previousLine.intersects(nextLine, &intersection);
        if (join == JoinMode::Flat) {
            const PkLineF shortcut(previousLine.p2(), nextLine.p1());
            const qreal angle = shortcut.angleTo(previousLine);
            if (type == PkLineF::BoundedIntersection
                || (angle > 90 && !fuzzyCompare(angle, qreal(90)))) {
                outputLineTo(focalX, focalY);
                outputLineTo(nextLine.x1(), nextLine.y1());
                return;
            }
            outputLineTo(nextLine.x1(), nextLine.y1());
        } else if (join == JoinMode::Miter) {
            const qreal appliedLimit = m_strokeWidth * m_miterLimit;
            const PkLineF shortcut(previousLine.p2(), nextLine.p1());
            const qreal angle = shortcut.angleTo(previousLine);
            if (type == PkLineF::BoundedIntersection
                || (angle > 90 && !fuzzyCompare(angle, qreal(90)))) {
                outputLineTo(focalX, focalY);
                outputLineTo(nextLine.x1(), nextLine.y1());
                return;
            }
            const PkLineF miterLine(PkPointF(m_back1X, m_back1Y), intersection);
            if (type == PkLineF::NoIntersection || miterLine.length() > appliedLimit) {
                PkLineF line1(previousLine);
                line1.setLength(appliedLimit);
                line1.translate(previousLine.dx(), previousLine.dy());
                PkLineF line2(nextLine);
                line2.setLength(appliedLimit);
                line2.translate(-line2.dx(), -line2.dy());
                outputLineTo(line1.x2(), line1.y2());
                outputLineTo(line2.x1(), line2.y1());
                outputLineTo(nextLine.x1(), nextLine.y1());
            } else {
                outputLineTo(intersection.x(), intersection.y());
                outputLineTo(nextLine.x1(), nextLine.y1());
            }
        } else if (join == JoinMode::Square) {
            const Fixed offset = m_strokeWidth / 2;
            PkLineF line1(previousLine);
            const qreal dot = previousLine.dx() * nextLine.dx()
                + previousLine.dy() * nextLine.dy();
            if (dot > 0) line1 = PkLineF(previousLine.p2(), previousLine.p1());
            else line1.translate(line1.dx(), line1.dy());
            line1.setLength(offset);
            PkLineF line2(nextLine.p2(), nextLine.p1());
            line2.translate(line2.dx(), line2.dy());
            line2.setLength(offset);
            outputLineTo(line1.x2(), line1.y2());
            outputLineTo(line2.x2(), line2.y2());
            outputLineTo(line2.x1(), line2.y1());
        } else if (join == JoinMode::Round) {
            const Fixed offset = m_strokeWidth / 2;
            const PkLineF shortcut(previousLine.p2(), nextLine.p1());
            const qreal angle = shortcut.angleTo(previousLine);
            if ((type == PkLineF::BoundedIntersection || angle > 90.01)
                && nextLine.length() > offset) {
                outputLineTo(focalX, focalY);
                outputLineTo(nextLine.x1(), nextLine.y1());
                return;
            }
            const qreal firstAngle = angleOnX(previousLine);
            const qreal secondAngle = angleOnX(nextLine);
            const qreal sweep = std::abs(secondAngle - firstAngle);
            PkPointF curves[15];
            int pointCount = 0;
            curvesForArc({focalX - offset, focalY - offset,
                          offset * 2, offset * 2},
                         firstAngle + 90, -sweep, curves, &pointCount);
            for (int i = 0; i < pointCount; i += 3) {
                outputCubicTo(curves[i].x(), curves[i].y(),
                              curves[i + 1].x(), curves[i + 1].y(),
                              curves[i + 2].x(), curves[i + 2].y());
            }
            outputLineTo(nextLine.x1(), nextLine.y1());
        } else if (join == JoinMode::RoundCap) {
            const Fixed offset = m_strokeWidth / 2;
            PkLineF line1(previousLine);
            const qreal dot = previousLine.dx() * nextLine.dx()
                + previousLine.dy() * nextLine.dy();
            if (dot > 0) line1 = PkLineF(previousLine.p2(), previousLine.p1());
            else line1.translate(line1.dx(), line1.dy());
            line1.setLength(pathKappa * offset);
            PkLineF line2(focalX, focalY, previousLine.x2(), previousLine.y2());
            line2.translate(-line2.dy(), line2.dx());
            line2.setLength(pathKappa * offset);
            outputCubicTo(line1.x2(), line1.y2(), line2.x2(), line2.y2(),
                          line2.x1(), line2.y1());
            line2 = PkLineF(line2.x1(), line2.y1(),
                            line2.x1() - line2.dx(), line2.y1() - line2.dy());
            line1.translate(nextLine.x1() - line1.x1(), nextLine.y1() - line1.y1());
            outputCubicTo(line2.x2(), line2.y2(), line1.x2(), line1.y2(),
                          line1.x1(), line1.y1());
        } else if (join == JoinMode::SvgMiter) {
            const PkLineF shortcut(previousLine.p2(), nextLine.p1());
            const qreal angle = shortcut.angleTo(previousLine);
            if (type == PkLineF::BoundedIntersection
                || (angle > 90 && !fuzzyCompare(angle, qreal(90)))) {
                outputLineTo(focalX, focalY);
                outputLineTo(nextLine.x1(), nextLine.y1());
                return;
            }
            const PkLineF miterLine(PkPointF(focalX, focalY), intersection);
            if (type == PkLineF::NoIntersection
                || miterLine.length() > m_strokeWidth * m_miterLimit / 2) {
                outputLineTo(nextLine.x1(), nextLine.y1());
            } else {
                outputLineTo(intersection.x(), intersection.y());
                outputLineTo(nextLine.x1(), nextLine.y1());
            }
        }
    }

protected:
    void processCurrentSubpath() override
    {
        ForwardIterator forward(m_elements);
        BackwardIterator backward(m_elements);
        PkLineF forwardTangent;
        PkLineF backwardTangent;
        const bool forwardClosed = strokeSide(&forward, this, false, &forwardTangent);
        const bool backwardClosed = strokeSide(&backward, this, !forwardClosed, &backwardTangent);
        if (!backwardClosed && !forwardTangent.isNull()) {
            joinPoints(m_elements.front().x, m_elements.front().y,
                       forwardTangent, m_capStyle);
        }
    }

private:
    Fixed m_strokeWidth = 1;
    Fixed m_miterLimit = 2;
    JoinMode m_capStyle = JoinMode::Square;
    JoinMode m_joinStyle = JoinMode::Flat;
    Fixed m_back1X = 0;
    Fixed m_back1Y = 0;
    Fixed m_back2X = 0;
    Fixed m_back2Y = 0;
};

template <class Iterator>
bool strokeSide(Iterator *iterator, Stroker *stroker,
                bool capFirst, PkLineF *startTangent)
{
    std::array<Bezier, 16> offsetCurves;
    const auto firstElement = iterator->next();
    FixedPoint start = firstElement;
    FixedPoint previous = start;
    bool first = true;
    const Fixed offset = stroker->strokeWidth() / 2;
    while (iterator->hasNext()) {
        const auto element = iterator->next();
        if (element.isLineTo()) {
            PkLineF line(previous.x, previous.y, element.x, element.y);
            if (line.p1() != line.p2()) {
                PkLineF normal = line.normalVector();
                normal.setLength(offset);
                line.translate(normal.dx(), normal.dy());
                if (first) {
                    if (capFirst) stroker->joinPoints(previous.x, previous.y, line,
                                                      stroker->capStyleMode());
                    else stroker->outputMoveTo(line.x1(), line.y1());
                    *startTangent = line;
                    first = false;
                } else {
                    stroker->joinPoints(previous.x, previous.y, line,
                                        stroker->joinStyleMode());
                }
                stroker->outputLineTo(line.x2(), line.y2());
                previous = element;
            }
        } else if (element.isCurveTo()) {
            const auto control2 = iterator->next();
            const auto endPoint = iterator->next();
            const Bezier bezier = Bezier::fromPoints(
                PkPointF(previous.x, previous.y), PkPointF(element.x, element.y),
                PkPointF(control2.x, control2.y), PkPointF(endPoint.x, endPoint.y));
            const int count = bezier.shifted(offsetCurves.data(), int(offsetCurves.size()),
                                             offset, stroker->curveThreshold());
            if (count) {
                PkLineF tangent = bezier.startTangent();
                tangent.translate(offsetCurves[0].pt1() - bezier.pt1());
                if (first) {
                    const PkPointF point = offsetCurves[0].pt1();
                    if (capFirst) stroker->joinPoints(previous.x, previous.y, tangent,
                                                      stroker->capStyleMode());
                    else stroker->outputMoveTo(point.x(), point.y());
                    *startTangent = tangent;
                    first = false;
                } else {
                    stroker->joinPoints(previous.x, previous.y, tangent,
                                        stroker->joinStyleMode());
                }
                for (int i = 0; i < count; ++i) {
                    stroker->outputCubicTo(offsetCurves[i].x2, offsetCurves[i].y2,
                                           offsetCurves[i].x3, offsetCurves[i].y3,
                                           offsetCurves[i].x4, offsetCurves[i].y4);
                }
            }
            previous = endPoint;
        }
    }
    if (start == previous) {
        if (!first) stroker->joinPoints(previous.x, previous.y, *startTangent,
                                        stroker->joinStyleMode());
        return true;
    }
    return false;
}

void dashMoveTo(Fixed x, Fixed y, void *data)
{
    static_cast<Stroker *>(data)->moveTo(x, y);
}
void dashLineTo(Fixed x, Fixed y, void *data)
{
    static_cast<Stroker *>(data)->lineTo(x, y);
}
void dashCubicTo(Fixed, Fixed, Fixed, Fixed, Fixed, Fixed, void *)
{
}

bool lineRectIntersectsRect(FixedPoint p1, FixedPoint p2,
                            const FixedPoint &topLeft, const FixedPoint &bottomRight)
{
    return ((p1.x > topLeft.x || p2.x > topLeft.x)
            && (p1.x < bottomRight.x || p2.x < bottomRight.x)
            && (p1.y > topLeft.y || p2.y > topLeft.y)
            && (p1.y < bottomRight.y || p2.y < bottomRight.y));
}

bool lineIntersectsRect(FixedPoint p1, FixedPoint p2,
                        const FixedPoint &topLeft, const FixedPoint &bottomRight)
{
    if (!lineRectIntersectsRect(p1, p2, topLeft, bottomRight)) return false;
    if (p1.x == p2.x || p1.y == p2.y) return true;
    if (p1.y > p2.y) std::swap(p1, p2);
    FixedPoint u;
    FixedPoint v;
    const FixedPoint w{p2.x - p1.x, p2.y - p1.y};
    if (p1.x < p2.x) {
        u = {topLeft.x - p1.x, bottomRight.y - p1.y};
        v = {bottomRight.x - p1.x, topLeft.y - p1.y};
    } else {
        u = {topLeft.x - p1.x, topLeft.y - p1.y};
        v = {bottomRight.x - p1.x, bottomRight.y - p1.y};
    }
    const qreal value1 = u.x * w.y - u.y * w.x;
    const qreal value2 = v.x * w.y - v.y * w.x;
    return (value1 < 0 && value2 > 0) || (value1 > 0 && value2 < 0);
}

class DashStroker : public StrokerOps
{
public:
    explicit DashStroker(Stroker *stroker) : m_stroker(stroker)
    {
        setHooks(dashMoveTo, dashLineTo, dashCubicTo);
    }
    void setDashPattern(const std::vector<qreal> &pattern)
    {
        m_dashPattern.clear();
        m_dashPattern.reserve(pattern.size());
        for (qreal value : pattern) m_dashPattern.push_back(value);
    }
    void setDashOffset(qreal offset) { m_dashOffset = offset; }
    void begin(void *data) override
    {
        m_stroker->begin(data);
        StrokerOps::begin(data);
    }
    void end() override
    {
        StrokerOps::end();
        m_stroker->end();
    }

protected:
    void processCurrentSubpath() override
    {
        int dashCount = std::min(int(m_dashPattern.size()), 32);
        std::array<Fixed, 32> dashes{};
        m_customData = m_stroker;
        const qreal strokeWidth = m_stroker->strokeWidth();
        const qreal miterLimit = m_stroker->miterLimit();
        qreal longestLength = 0;
        qreal sumLength = 0;
        for (int i = 0; i < dashCount; ++i) {
            dashes[std::size_t(i)] = std::max(m_dashPattern[std::size_t(i)], qreal(0))
                * strokeWidth;
            sumLength += dashes[std::size_t(i)];
            longestLength = std::max(longestLength, dashes[std::size_t(i)]);
        }
        if (fuzzyIsNull(sumLength)) return;
        const qreal inverseSum = 1.0 / sumLength;
        dashCount &= -2;
        int dashIndex = 0;
        qreal position = 0;
        qreal dashOffset = m_dashOffset * strokeWidth;
        dashOffset -= std::floor(dashOffset * inverseSum) * sumLength;
        while (dashOffset >= dashes[std::size_t(dashIndex)]) {
            dashOffset -= dashes[std::size_t(dashIndex)];
            if (++dashIndex >= dashCount) dashIndex = 0;
        }
        qreal elementStart = 0;
        FlatIterator iterator(m_elements, m_dashThreshold);
        FixedPoint previous = iterator.next();
        if (!previous.isFinite()) return;
        const bool clipping = !m_clipRect.isEmpty();
        FixedPoint movePosition = previous;
        FixedPoint linePosition;
        const Fixed padding = std::max(strokeWidth, miterLimit) * longestLength;
        const FixedPoint clipTopLeft{m_clipRect.left() - padding,
                                     m_clipRect.top() - padding};
        const FixedPoint clipBottomRight{m_clipRect.right() + padding,
                                         m_clipRect.bottom() + padding};
        bool hasMoveTo = false;
        while (iterator.hasNext()) {
            const auto element = iterator.next();
            if (!FixedPoint(element).isFinite()) continue;
            const PkLineF line(previous.x, previous.y, element.x, element.y);
            qreal elementLength = line.length();
            const qreal elementStop = elementStart + elementLength;
            bool done = position >= elementStop;
            const bool clipIt = clipping
                && !lineIntersectsRect(previous, element, clipTopLeft, clipBottomRight);
            const bool skipDashing = elementLength * inverseSum > 10000;
            int maxDashes = dashCount;
            if (skipDashing || clipIt) {
                elementLength -= std::floor(elementLength * inverseSum) * sumLength;
                while (!done) {
                    const qreal dashPosition = position + dashes[std::size_t(dashIndex)]
                        - dashOffset - elementStart;
                    if (dashPosition > elementLength) {
                        dashOffset = dashes[std::size_t(dashIndex)]
                            - (dashPosition - elementLength);
                        position = elementStop;
                        done = true;
                    } else {
                        position = --maxDashes > 0 ? dashPosition + elementStart : elementStop;
                        done = position >= elementStop;
                        if (++dashIndex >= dashCount) dashIndex = 0;
                        dashOffset = 0;
                    }
                }
                if (clipIt) {
                    hasMoveTo = false;
                } else {
                    if (!hasMoveTo) {
                        emitMoveTo(movePosition.x, movePosition.y);
                        hasMoveTo = true;
                    }
                    emitLineTo(element.x, element.y);
                }
                movePosition = element;
            }
            while (!done) {
                PkPointF point2;
                const bool hasOffset = dashOffset > 0;
                const bool evenDash = (dashIndex & 1) == 0;
                const qreal dashPosition = position + dashes[std::size_t(dashIndex)]
                    - dashOffset - elementStart;
                if (dashPosition > elementLength) {
                    dashOffset = dashes[std::size_t(dashIndex)]
                        - (dashPosition - elementLength);
                    position = elementStop;
                    done = true;
                    point2 = line.p2();
                } else {
                    point2 = line.pointAt(dashPosition / elementLength);
                    position = dashPosition + elementStart;
                    done = position >= elementStop;
                    if (++dashIndex >= dashCount) dashIndex = 0;
                    dashOffset = 0;
                }
                if (evenDash) {
                    linePosition = {point2.x(), point2.y()};
                    if (!clipping || lineRectIntersectsRect(movePosition, linePosition,
                                                            clipTopLeft, clipBottomRight)) {
                        if (!hasOffset || !hasMoveTo) {
                            emitMoveTo(movePosition.x, movePosition.y);
                            hasMoveTo = true;
                        }
                        emitLineTo(linePosition.x, linePosition.y);
                    } else {
                        hasMoveTo = false;
                    }
                    movePosition = linePosition;
                } else {
                    movePosition = {point2.x(), point2.y()};
                }
            }
            elementStart = elementStop;
            previous = element;
        }
    }

private:
    Stroker *m_stroker;
    std::vector<Fixed> m_dashPattern;
    qreal m_dashOffset = 0;
};

void pathMoveTo(Fixed x, Fixed y, void *data)
{
    static_cast<PkPainterPath *>(data)->moveTo(x, y);
}
void pathLineTo(Fixed x, Fixed y, void *data)
{
    static_cast<PkPainterPath *>(data)->lineTo(x, y);
}
void pathCubicTo(Fixed x1, Fixed y1, Fixed x2, Fixed y2,
                 Fixed x3, Fixed y3, void *data)
{
    static_cast<PkPainterPath *>(data)->cubicTo(x1, y1, x2, y2, x3, y3);
}

} // namespace

PkPainterPath createStrokeOutline(const PkPainterPath &path,
                                  const PkPen &pen,
                                  const PkRect &clip)
{
    PkPainterPath outline;
    if (path.isEmpty()) return outline;

    Stroker stroker;
    stroker.setHooks(pathMoveTo, pathLineTo, pathCubicTo);
    stroker.setStrokeWidth(pen.widthF() == 0 ? 1 : pen.widthF());
    stroker.setCapStyle(pen.capStyle());
    stroker.setJoinStyle(pen.joinStyle());
    stroker.setMiterLimit(pen.miterLimit());

    const std::vector<qreal> pattern = pen.dashPattern();
    if (pattern.empty()) {
        stroker.strokePath(path, &outline);
    } else {
        DashStroker dashStroker(&stroker);
        dashStroker.setDashPattern(pattern);
        dashStroker.setDashOffset(pen.dashOffset());
        dashStroker.setClipRect({qreal(clip.x()), qreal(clip.y()),
                                qreal(clip.width()), qreal(clip.height())});
        dashStroker.strokePath(path, &outline);
    }
    outline.setFillRule(Qt::WindingFill);
    return outline;
}

} // namespace KisPathRasterizer::Private
