// Real Qt 5.15 QPainterPath <-> PkPainterPath path-operation oracle (R-39).
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QtGlobal>

#include <algorithm>
#include <cassert>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(QPainterPath) || defined(QPointF) || defined(QRectF)
#  error "pathops oracle resolved a compat shim instead of real Qt"
#endif

namespace pkoracle {
#include "PkPoint.h"
#include "PkGlobal.cpp"
#include "PkPoint.cpp"
#include "PkSize.h"
#include "PkSize.cpp"
#include "PkRect.h"
#include "PkRect.cpp"
#include "PkTransform.h"
#include "PkTransform.cpp"
#include "PkLine.h"
#include "PkLine.cpp"
#include "PkPolygon.h"
#include "PkPolygon.cpp"
#include "PkPainterPath.h"
#include "PkPainterPath.cpp"
} // namespace pkoracle

static_assert(!std::is_same<QPainterPath, pkoracle::PkPainterPath>::value,
              "oracle sides must be distinct");

namespace {

enum class ShapeKind {
    Empty,
    MoveOnly,
    Rectangle,
    Ellipse,
    OpenPolyline,
    ClosedPolyline,
    NestedRings,
    DisjointCompound,
    BowTie,
    Star
};

const char *shapeName(ShapeKind kind)
{
    switch (kind) {
    case ShapeKind::Empty: return "empty";
    case ShapeKind::MoveOnly: return "move-only";
    case ShapeKind::Rectangle: return "rectangle";
    case ShapeKind::Ellipse: return "ellipse";
    case ShapeKind::OpenPolyline: return "open-polyline";
    case ShapeKind::ClosedPolyline: return "closed-polyline";
    case ShapeKind::NestedRings: return "nested-rings";
    case ShapeKind::DisjointCompound: return "disjoint-compound";
    case ShapeKind::BowTie: return "bow-tie";
    case ShapeKind::Star: return "star";
    }
    return "unknown";
}

QPainterPath makeQt(ShapeKind kind, Qt::FillRule rule)
{
    QPainterPath path;
    if (kind == ShapeKind::Empty)
        return path;
    path.setFillRule(rule);
    switch (kind) {
    case ShapeKind::Empty:
        break;
    case ShapeKind::MoveOnly:
        path.moveTo(17, -23);
        break;
    case ShapeKind::Rectangle:
        path.addRect(QRectF(0, 0, 9, 8));
        break;
    case ShapeKind::Ellipse:
        path.addEllipse(QRectF(2, -1, 9, 10));
        break;
    case ShapeKind::OpenPolyline:
        path.moveTo(-1, 1); path.lineTo(6, 11); path.lineTo(12, 2);
        break;
    case ShapeKind::ClosedPolyline:
        path.moveTo(-2, 3); path.lineTo(4, -2); path.lineTo(12, 4);
        path.lineTo(6, 12); path.closeSubpath();
        break;
    case ShapeKind::NestedRings:
        path.addRect(QRectF(-1, -1, 13, 13));
        path.addRect(QRectF(3, 3, 5, 5));
        break;
    case ShapeKind::DisjointCompound:
        path.addRect(QRectF(-2, -2, 4, 4));
        path.addEllipse(QRectF(8, 7, 5, 6));
        break;
    case ShapeKind::BowTie:
        path.moveTo(0, 0); path.lineTo(11, 11); path.lineTo(0, 11);
        path.lineTo(11, 0); path.closeSubpath();
        break;
    case ShapeKind::Star:
        path.moveTo(5, -2); path.lineTo(7, 4); path.lineTo(13, 4);
        path.lineTo(8, 7); path.lineTo(10, 13); path.lineTo(5, 9);
        path.lineTo(0, 13); path.lineTo(2, 7); path.lineTo(-3, 4);
        path.lineTo(3, 4); path.closeSubpath();
        break;
    }
    return path;
}

pkoracle::PkPainterPath makePk(ShapeKind kind, pkoracle::Qt::FillRule rule)
{
    using namespace pkoracle;
    PkPainterPath path;
    if (kind == ShapeKind::Empty)
        return path;
    path.setFillRule(rule);
    switch (kind) {
    case ShapeKind::Empty:
        break;
    case ShapeKind::MoveOnly:
        path.moveTo(17, -23);
        break;
    case ShapeKind::Rectangle:
        path.addRect(PkRectF(0, 0, 9, 8));
        break;
    case ShapeKind::Ellipse:
        path.addEllipse(PkRectF(2, -1, 9, 10));
        break;
    case ShapeKind::OpenPolyline:
        path.moveTo(-1, 1); path.lineTo(6, 11); path.lineTo(12, 2);
        break;
    case ShapeKind::ClosedPolyline:
        path.moveTo(-2, 3); path.lineTo(4, -2); path.lineTo(12, 4);
        path.lineTo(6, 12); path.closeSubpath();
        break;
    case ShapeKind::NestedRings:
        path.addRect(PkRectF(-1, -1, 13, 13));
        path.addRect(PkRectF(3, 3, 5, 5));
        break;
    case ShapeKind::DisjointCompound:
        path.addRect(PkRectF(-2, -2, 4, 4));
        path.addEllipse(PkRectF(8, 7, 5, 6));
        break;
    case ShapeKind::BowTie:
        path.moveTo(0, 0); path.lineTo(11, 11); path.lineTo(0, 11);
        path.lineTo(11, 0); path.closeSubpath();
        break;
    case ShapeKind::Star:
        path.moveTo(5, -2); path.lineTo(7, 4); path.lineTo(13, 4);
        path.lineTo(8, 7); path.lineTo(10, 13); path.lineTo(5, 9);
        path.lineTo(0, 13); path.lineTo(2, 7); path.lineTo(-3, 4);
        path.lineTo(3, 4); path.closeSubpath();
        break;
    }
    return path;
}

bool sameCoordinate(double lhs, double rhs)
{
    return std::abs(lhs - rhs) <= 0.0000001;
}

std::string coordinateToken(double value)
{
    if (std::abs(value) < 0.00000005)
        value = 0.0;
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(7) << value;
    return stream.str();
}

std::string pointToken(double x, double y)
{
    return coordinateToken(x) + "," + coordinateToken(y);
}

std::string joinEdges(const std::vector<std::string> &edges, std::size_t start)
{
    std::string joined;
    for (std::size_t i = 0; i < edges.size(); ++i) {
        if (!joined.empty())
            joined += '|';
        joined += edges[(start + i) % edges.size()];
    }
    return joined;
}

template <typename Path>
std::vector<std::string> signature(const Path &path)
{
    std::vector<std::string> subpaths;
    std::string startPoint;
    std::string currentPoint;
    std::vector<std::string> edges;
    bool haveSubpath = false;

    const auto finishSubpath = [&]() {
        if (!haveSubpath)
            return;
        if (edges.empty()) {
            subpaths.push_back("M:" + startPoint);
        } else if (currentPoint == startPoint) {
            std::string best = joinEdges(edges, 0);
            for (std::size_t rotation = 1; rotation < edges.size(); ++rotation)
                best = std::min(best, joinEdges(edges, rotation));
            subpaths.push_back("C:" + best);
        } else {
            subpaths.push_back("O:M:" + startPoint + "|" + joinEdges(edges, 0));
        }
    };

    for (int i = 0; i < path.elementCount(); ++i) {
        const auto element = path.elementAt(i);
        const int type = int(element.type);
        const std::string point = pointToken(element.x, element.y);
        if (type == 0) {
            finishSubpath();
            edges.clear();
            startPoint = currentPoint = point;
            haveSubpath = true;
        } else if (type == 1 && haveSubpath) {
            edges.push_back("L:" + currentPoint + ">" + point);
            currentPoint = point;
        } else if (type == 2 && haveSubpath && i + 2 < path.elementCount()
                   && int(path.elementAt(i + 1).type) == 3
                   && int(path.elementAt(i + 2).type) == 3) {
            const auto control2 = path.elementAt(i + 1);
            const auto end = path.elementAt(i + 2);
            const std::string endPoint = pointToken(end.x, end.y);
            edges.push_back("B:" + currentPoint + ">" + point + ">" +
                            pointToken(control2.x, control2.y) + ">" + endPoint);
            currentPoint = endPoint;
            i += 2;
        } else {
            edges.push_back("INVALID:" + std::to_string(type) + ":" + point);
        }
    }
    finishSubpath();
    std::sort(subpaths.begin(), subpaths.end());
    return subpaths;
}

std::uint64_t total = 0;
std::uint64_t mismatches = 0;

void rec(bool equal, const std::string &tag)
{
    ++total;
    if (!equal) {
        ++mismatches;
        std::cout << "DIFFTAG " << tag << " 1\n";
    }
}

std::string baseTag(const char *operation, ShapeKind a, ShapeKind b,
                    int fillA, int fillB)
{
    return std::string(operation) + ":a=" + shapeName(a) + ":b=" + shapeName(b) +
           ":fa=" + std::to_string(fillA) + ":fb=" + std::to_string(fillB);
}

void compareResult(const QPainterPath &qt, const pkoracle::PkPainterPath &pk,
                   const std::string &tag)
{
    rec(qt.isEmpty() == pk.isEmpty(), tag + ":field=empty");
    rec(int(qt.fillRule()) == int(pk.fillRule()), tag + ":field=fill-rule");
    const QRectF qr = qt.boundingRect();
    const pkoracle::PkRectF pr = pk.boundingRect();
    rec(sameCoordinate(qr.x(), pr.x()), tag + ":field=bounds-x");
    rec(sameCoordinate(qr.y(), pr.y()), tag + ":field=bounds-y");
    rec(sameCoordinate(qr.width(), pr.width()), tag + ":field=bounds-width");
    rec(sameCoordinate(qr.height(), pr.height()), tag + ":field=bounds-height");
    rec(signature(qt) == signature(pk), tag + ":field=normalized-elements");

    for (int yi = -2; yi <= 14; ++yi) {
        for (int xi = -3; xi <= 14; ++xi) {
            const double x = xi + 0.371;
            const double y = yi + 0.619;
            rec(qt.contains(QPointF(x, y)) == pk.contains(pkoracle::PkPointF(x, y)),
                tag + ":field=membership:x=" + std::to_string(xi) +
                ":y=" + std::to_string(yi));
        }
    }
}

enum class BinaryOp { NamedAnd, NamedOr, NamedSub, OperatorAnd, OperatorOr, OperatorAdd, OperatorSub };

const char *opName(BinaryOp op)
{
    switch (op) {
    case BinaryOp::NamedAnd: return "intersected";
    case BinaryOp::NamedOr: return "united";
    case BinaryOp::NamedSub: return "subtracted";
    case BinaryOp::OperatorAnd: return "operator-and";
    case BinaryOp::OperatorOr: return "operator-or";
    case BinaryOp::OperatorAdd: return "operator-add";
    case BinaryOp::OperatorSub: return "operator-sub";
    }
    return "unknown";
}

void compareBinary(BinaryOp op, ShapeKind a, ShapeKind b, int fillA, int fillB)
{
    const QPainterPath qa = makeQt(a, fillA ? Qt::WindingFill : Qt::OddEvenFill);
    const QPainterPath qb = makeQt(b, fillB ? Qt::WindingFill : Qt::OddEvenFill);
    const auto pa = makePk(a, fillA ? pkoracle::Qt::WindingFill : pkoracle::Qt::OddEvenFill);
    const auto pb = makePk(b, fillB ? pkoracle::Qt::WindingFill : pkoracle::Qt::OddEvenFill);

    QPainterPath qr;
    pkoracle::PkPainterPath pr;
    switch (op) {
    case BinaryOp::NamedAnd: qr = qa.intersected(qb); pr = pa.intersected(pb); break;
    case BinaryOp::NamedOr: qr = qa.united(qb); pr = pa.united(pb); break;
    case BinaryOp::NamedSub: qr = qa.subtracted(qb); pr = pa.subtracted(pb); break;
    case BinaryOp::OperatorAnd: qr = qa & qb; pr = pa & pb; break;
    case BinaryOp::OperatorOr: qr = qa | qb; pr = pa | pb; break;
    case BinaryOp::OperatorAdd: qr = qa + qb; pr = pa + pb; break;
    case BinaryOp::OperatorSub: qr = qa - qb; pr = pa - pb; break;
    }
    compareResult(qr, pr, baseTag(opName(op), a, b, fillA, fillB));
}

void compareNearCoincidentPair(double origin, double offset, const char *scaleTag)
{
    QPainterPath qa;
    QPainterPath qb;
    qa.addRect(QRectF(origin, 0, 1, 2));
    qb.addRect(QRectF(origin + offset, 0, 1, 2));

    pkoracle::PkPainterPath pa;
    pkoracle::PkPainterPath pb;
    pa.addRect(pkoracle::PkRectF(origin, 0, 1, 2));
    pb.addRect(pkoracle::PkRectF(origin + offset, 0, 1, 2));

    const std::string prefix = std::string("near-coincident:") + scaleTag;
    rec((qa == qb) == (pa == pb), prefix + ":field=input-equality");

    const BinaryOp operations[] = {
        BinaryOp::NamedAnd, BinaryOp::NamedOr, BinaryOp::NamedSub,
        BinaryOp::OperatorAnd, BinaryOp::OperatorOr,
        BinaryOp::OperatorAdd, BinaryOp::OperatorSub
    };
    for (BinaryOp op : operations) {
        QPainterPath qr;
        pkoracle::PkPainterPath pr;
        switch (op) {
        case BinaryOp::NamedAnd: qr = qa.intersected(qb); pr = pa.intersected(pb); break;
        case BinaryOp::NamedOr: qr = qa.united(qb); pr = pa.united(pb); break;
        case BinaryOp::NamedSub: qr = qa.subtracted(qb); pr = pa.subtracted(pb); break;
        case BinaryOp::OperatorAnd: qr = qa & qb; pr = pa & pb; break;
        case BinaryOp::OperatorOr: qr = qa | qb; pr = pa | pb; break;
        case BinaryOp::OperatorAdd: qr = qa + qb; pr = pa + pb; break;
        case BinaryOp::OperatorSub: qr = qa - qb; pr = pa - pb; break;
        }
        const std::string tag = prefix + ":operation=" + opName(op);
        compareResult(qr, pr, tag);
        const double probes[] = {origin + offset * 0.5, origin + 0.5,
                                 origin + 1 + offset * 0.5};
        for (int i = 0; i < 3; ++i) {
            rec(qr.contains(QPointF(probes[i], 1.0)) ==
                    pr.contains(pkoracle::PkPointF(probes[i], 1.0)),
                tag + ":field=local-membership:probe=" + std::to_string(i));
        }
    }
}

void compareAdversarialCubics()
{
    const pkoracle::PkArcBezier cancellation = pkoracle::PkArcBezier::fromPoints(
        pkoracle::PkPointF(1.0e16, 0), pkoracle::PkPointF(-1.0e16, 0),
        pkoracle::PkPointF(1.0e16, 0), pkoracle::PkPointF(-1.0e16, 0));
    rec(sameCoordinate(cancellation.pointAt(0.499999999999).x(),
                       7.999829140122841e-20),
        "oracle-self:bezier-point-at:case=cancellation");

    const pkoracle::PkArcBezier large = pkoracle::PkArcBezier::fromPoints(
        pkoracle::PkPointF(1.0e18, 0), pkoracle::PkPointF(-1.0e18, 0),
        pkoracle::PkPointF(1.0e18, 0), pkoracle::PkPointF(-1.0e18, 0));
    rec(sameCoordinate(large.pointAt(0.3).x(), 6.3999999999999976e16),
        "oracle-self:bezier-point-at:case=large-alternating");

    QPainterPath qt;
    qt.moveTo(1.0e15, 0);
    qt.cubicTo(-1.0e15, 3, 1.0e15, -3, -1.0e15, 0);
    qt.lineTo(-1.0e15, 4);
    qt.lineTo(1.0e15, 4);
    qt.closeSubpath();

    pkoracle::PkPainterPath pk;
    pk.moveTo(1.0e15, 0);
    pk.cubicTo(pkoracle::PkPointF(-1.0e15, 3), pkoracle::PkPointF(1.0e15, -3),
               pkoracle::PkPointF(-1.0e15, 0));
    pk.lineTo(-1.0e15, 4);
    pk.lineTo(1.0e15, 4);
    pk.closeSubpath();

    compareResult(qt.simplified(), pk.simplified(),
                  "adversarial-cubic:operation=simplified");
}

} // namespace

int main()
{
    QPainterPath topologyA;
    topologyA.moveTo(0, 0); topologyA.lineTo(2, 0); topologyA.lineTo(0, 2);
    topologyA.moveTo(10, 10); topologyA.lineTo(12, 10); topologyA.lineTo(10, 12);

    QPainterPath rewired = topologyA;
    rewired.setElementPositionAt(2, 10, 12);
    rewired.setElementPositionAt(5, 0, 2);
    rec(signature(topologyA) != signature(rewired),
        "oracle-self:normalized-elements:mutation=rewire");

    QPainterPath reordered = topologyA;
    reordered.setElementPositionAt(1, 0, 2);
    reordered.setElementPositionAt(2, 2, 0);
    rec(signature(topologyA) != signature(reordered),
        "oracle-self:normalized-elements:mutation=reorder");

    const ShapeKind kinds[] = {
        ShapeKind::Empty, ShapeKind::MoveOnly,
        ShapeKind::Rectangle, ShapeKind::Ellipse, ShapeKind::OpenPolyline,
        ShapeKind::ClosedPolyline, ShapeKind::NestedRings,
        ShapeKind::DisjointCompound, ShapeKind::BowTie, ShapeKind::Star
    };
    const BinaryOp operations[] = {
        BinaryOp::NamedAnd, BinaryOp::NamedOr, BinaryOp::NamedSub,
        BinaryOp::OperatorAnd, BinaryOp::OperatorOr,
        BinaryOp::OperatorAdd, BinaryOp::OperatorSub
    };

    for (ShapeKind kind : kinds)
        std::cout << "FAMILY " << shapeName(kind) << '\n';
    std::cout << "FAMILY adversarial-cubic\n";

    for (int fillA = 0; fillA < 2; ++fillA) {
        for (ShapeKind a : kinds) {
            const QPainterPath qa = makeQt(a, fillA ? Qt::WindingFill : Qt::OddEvenFill);
            const auto pa = makePk(a, fillA ? pkoracle::Qt::WindingFill : pkoracle::Qt::OddEvenFill);
            compareResult(qa.simplified(), pa.simplified(),
                          baseTag("simplified", a, a, fillA, fillA));
            for (int fillB = 0; fillB < 2; ++fillB) {
                for (ShapeKind b : kinds) {
                    const QPainterPath qb = makeQt(b, fillB ? Qt::WindingFill : Qt::OddEvenFill);
                    const auto pb = makePk(b, fillB ? pkoracle::Qt::WindingFill : pkoracle::Qt::OddEvenFill);
                    const std::string relation = baseTag("relation", a, b, fillA, fillB);
                    rec((qa == qb) == (pa == pb), relation + ":field=path-equality");
                    rec(qa.contains(qb) == pa.contains(pb), relation + ":field=contains-path");
                    rec(qa.intersects(qb) == pa.intersects(pb), relation + ":field=intersects-path");
                    for (BinaryOp op : operations)
                        compareBinary(op, a, b, fillA, fillB);
                }
            }
        }
    }


    compareNearCoincidentPair(10.0, 0.1, "small");
    compareNearCoincidentPair(1.0e6, 0.0000005, "large");
    compareAdversarialCubics();

    std::cout << "DIFF total=" << total << " mismatch=" << mismatches << '\n';
    return 0;
}
