/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QBrush>
#include <QPainter>
#include <QPen>
#include <QPolygonF>

#include "PkFlakeBridge.h"
#include "PkQPainterAdapter.h"

namespace
{

template<class... Visitors>
struct Overloaded : Visitors...
{
    using Visitors::operator()...;
};

template<class... Visitors>
Overloaded(Visitors...) -> Overloaded<Visitors...>;

QBrush toQBrush(const PkBrush &brush)
{
    QBrush result(toQColor(brush.color()));
    result.setStyle(brush.style());
    return result;
}

QPen toQPen(const PkPen &pen)
{
    QPen result(toQBrush(pen.brush()), pen.widthF(), pen.style(), pen.capStyle());
    if (!pen.dashPattern().empty()) {
        QVector<qreal> pattern;
        pattern.reserve(static_cast<int>(pen.dashPattern().size()));
        for (qreal length : pen.dashPattern()) {
            pattern.append(length);
        }
        result.setDashPattern(pattern);
    }
    result.setCosmetic(pen.isCosmetic());
    return result;
}

QPolygonF toQPolygonF(const PkPolygonF &polygon)
{
    QPolygonF result;
    result.reserve(polygon.size());
    for (const PkPointF &point : polygon) {
        result.append(toQPointF(point));
    }
    return result;
}

}

PkQPainterAdapter::PkQPainterAdapter(QPainter &painter)
    : m_painter(painter)
{
}

void PkQPainterAdapter::submit(const PkPaintCommand &command)
{
    std::visit(Overloaded{
        [this](const PkSaveCommand &) {
            m_painter.save();
        },
        [this](const PkRestoreCommand &) {
            m_painter.restore();
        },
        [this](const PkSetPenCommand &value) {
            m_painter.setPen(toQPen(value.pen));
        },
        [this](const PkSetBrushCommand &value) {
            m_painter.setBrush(toQBrush(value.brush));
        },
        [this](const PkSetTransformCommand &value) {
            m_painter.setTransform(toQTransform(value.transform), value.combine);
        },
        [this](const PkSetRenderHintCommand &value) {
            m_painter.setRenderHint(static_cast<QPainter::RenderHint>(value.hint), value.enabled);
        },
        [this](const PkSetClipRectCommand &value) {
            m_painter.setClipRect(toQRectF(value.rect), value.operation);
        },
        [this](const PkDrawLineCommand &value) {
            m_painter.drawLine(QLineF(toQPointF(value.line.p1()), toQPointF(value.line.p2())));
        },
        [this](const PkDrawRectCommand &value) {
            m_painter.drawRect(toQRectF(value.rect));
        },
        [this](const PkDrawEllipseCommand &value) {
            m_painter.drawEllipse(toQRectF(value.rect));
        },
        [this](const PkDrawArcCommand &value) {
            m_painter.drawArc(toQRectF(value.rect), value.startAngle16, value.spanAngle16);
        },
        [this](const PkDrawPathCommand &value) {
            m_painter.drawPath(toQPainterPath(value.path));
        },
        [this](const PkDrawPolygonCommand &value) {
            m_painter.drawPolygon(toQPolygonF(value.polygon));
        },
        [this](const PkDrawImageCommand &value) {
            m_painter.drawImage(toQRectF(value.target), toQImage(value.image));
        }
    }, command);
}
