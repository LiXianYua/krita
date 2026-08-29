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
    path.setFillRule(rule);
    switch (kind) {
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
    path.setFillRule(rule);
    switch (kind) {
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

std::int64_t quantize(double value)
{
    return static_cast<std::int64_t>(std::llround(value * 10000000.0));
}

std::vector<std::string> signature(const QPainterPath &path)
{
    std::vector<std::string> result;
    for (int i = 0; i < path.elementCount(); ++i) {
        const auto e = path.elementAt(i);
        result.push_back(std::to_string(int(e.type)) + ":" +
                         std::to_string(quantize(e.x)) + ":" +
                         std::to_string(quantize(e.y)));
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<std::string> signature(const pkoracle::PkPainterPath &path)
{
    std::vector<std::string> result;
    for (int i = 0; i < path.elementCount(); ++i) {
        const auto e = path.elementAt(i);
        result.push_back(std::to_string(int(e.type)) + ":" +
                         std::to_string(quantize(e.x)) + ":" +
                         std::to_string(quantize(e.y)));
    }
    std::sort(result.begin(), result.end());
    return result;
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
    rec(quantize(qr.x()) == quantize(pr.x()), tag + ":field=bounds-x");
    rec(quantize(qr.y()) == quantize(pr.y()), tag + ":field=bounds-y");
    rec(quantize(qr.width()) == quantize(pr.width()), tag + ":field=bounds-width");
    rec(quantize(qr.height()) == quantize(pr.height()), tag + ":field=bounds-height");
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

} // namespace

int main()
{
    const ShapeKind kinds[] = {
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
                    rec(qa.contains(qb) == pa.contains(pb), relation + ":field=contains-path");
                    rec(qa.intersects(qb) == pa.intersects(pb), relation + ":field=intersects-path");
                    for (BinaryOp op : operations)
                        compareBinary(op, a, b, fillA, fillB);
                }
            }
        }
    }

    std::cout << "DIFF total=" << total << " mismatch=" << mismatches << '\n';
    return 0;
}
