/* SPDX-FileCopyrightText: 2016 The Qt Company Ltd.
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: LGPL-3.0-only
 */
#pragma once

#include <PkPaintCommand.h>
#include "../shapes/ImageShapePngData.h"
#include <iomanip>
#include <locale>
#include <sstream>
#include <type_traits>
#include <cmath>

// Native vector output for SvgWriter's generic-shape painting fallback. An
// unsupported command invalidates the WHOLE document, allowing the existing
// raster fallback to run without silently exporting a partially drawn shape.
class PkSvgPainterBackend final : public PkPainterBackend
{
    struct State {
        PkPen pen;
        PkBrush brush;
        PkTransform transform;
        double opacity = 1;
        std::vector<std::string> clips;
    } m_state;
    std::vector<State> m_stack;
    PkRectF m_bounds;
    std::string m_defs, m_body;
    unsigned m_id = 0;
    bool m_supported = true;

    static std::string number(double value) {
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << std::setprecision(6) << value;
        return out.str();
    }
    static std::string matrix(const PkTransform &t) {
        return "matrix(" + number(t.m11()) + "," + number(t.m12()) + "," +
            number(t.m21()) + "," + number(t.m22()) + "," +
            number(t.dx()) + "," + number(t.dy()) + ")";
    }
    static std::string pathData(const PkPainterPath &path) {
        std::string out;
        PkPointF first;
        for (int i = 0; i < path.elementCount(); ++i) {
            const auto e = path.elementAt(i);
            if (e.isMoveTo()) {
                first = PkPointF(e.x, e.y);
                out += "M" + number(e.x) + " " + number(e.y);
            } else if (e.isLineTo()) {
                const bool last = i + 1 == path.elementCount() || path.elementAt(i + 1).isMoveTo();
                if (last && PkPointF(e.x, e.y) == first) out += "Z";
                else out += "L" + number(e.x) + " " + number(e.y);
            } else if (e.isCurveTo() && i + 2 < path.elementCount()) {
                const auto c = path.elementAt(++i), end = path.elementAt(++i);
                out += "C" + number(e.x) + " " + number(e.y) + " " +
                    number(c.x) + " " + number(c.y) + " " + number(end.x) + " " + number(end.y);
            }
        }
        return out;
    }
    static PkGradientStops svgStops(const PkGradient &gradient) {
        // qsvggenerator.cpp 5.15.7 saveGradientStops: SVG interpolates straight
        // colors, whereas Qt's default gradient interpolates premultiplied
        // colors. Preserve Qt's 0.02 stop subdivision and byte arithmetic.
        const auto stops = gradient.stops();
        bool constantAlpha = true;
        for (const auto &stop : stops) constantAlpha &= stop.color.alpha() == stops[0].color.alpha();
        if (constantAlpha) return stops;
        PkGradientStops result;
        const auto premul = [](unsigned value, unsigned alpha) {
            const unsigned product = value * alpha + 128;
            return (product + (product >> 8)) >> 8;
        };
        for (int i = 0; i + 1 < stops.size(); ++i) {
            const auto &a = stops[i]; const auto &b = stops[i + 1];
            result.append(a);
            const int parts = static_cast<int>(std::ceil((b.offset - a.offset) / .02));
            for (int j = 1; j < parts; ++j) {
                const unsigned t = 256 * j / parts;
                const unsigned alpha = (a.color.alpha() * (256 - t) + b.color.alpha() * t) >> 8;
                const auto channel = [&](unsigned av, unsigned bv) {
                    const unsigned value = (premul(av, a.color.alpha()) * (256 - t) + premul(bv, b.color.alpha()) * t) >> 8;
                    return alpha ? int((value * (0x00ff00ffu / alpha) + 0x8000) >> 16) : 0;
                };
                result.append(PkGradientStop(a.offset + j * ((b.offset - a.offset) / parts),
                    PkColor(channel(a.color.red(), b.color.red()), channel(a.color.green(), b.color.green()),
                            channel(a.color.blue(), b.color.blue()), alpha)));
            }
        }
        result.append(stops.back());
        return result;
    }
    std::string brushAttributes(const PkBrush &brush, const std::string &kind) {
        if (brush.style() == Pk::NoBrush) return " " + kind + "=\"none\"";
        if (const auto *gradient = brush.gradient()) {
            const auto type = gradient->type();
            if (type != PkGradient::LinearGradient && type != PkGradient::RadialGradient) {
                m_supported = false;
                return {};
            }
            const std::string id = "paint" + std::to_string(++m_id);
            const std::string tag = type == PkGradient::LinearGradient ? "linearGradient" : "radialGradient";
            m_defs += "<" + tag + " id=\"" + id + "\" gradientUnits=\"" +
                (gradient->coordinateMode() == PkGradient::ObjectBoundingMode ? "objectBoundingBox" : "userSpaceOnUse") + "\"";
            // QSvgGenerator does not serialize brush transform or spread.
            // This fallback preserves that existing export behavior.
            if (type == PkGradient::LinearGradient) {
                m_defs += " x1=\"" + number(gradient->start().x()) + "\" y1=\"" + number(gradient->start().y()) +
                    "\" x2=\"" + number(gradient->finalStop().x()) + "\" y2=\"" + number(gradient->finalStop().y()) + "\"";
            } else {
                m_defs += " cx=\"" + number(gradient->center().x()) + "\" cy=\"" + number(gradient->center().y()) +
                    "\" r=\"" + number(gradient->radius()) + "\" fx=\"" + number(gradient->focalPoint().x()) +
                    "\" fy=\"" + number(gradient->focalPoint().y()) + "\"";
            }
            m_defs += ">";
            for (const auto &stop : svgStops(*gradient))
                m_defs += "<stop offset=\"" + number(stop.offset) + "\" stop-color=\"" + stop.color.name().PkToUtf8() +
                    "\" stop-opacity=\"" + number(stop.color.alphaF()) + "\"/>";
            m_defs += "</" + tag + ">";
            return " " + kind + "=\"url(#" + id + ")\"";
        }
        if (brush.style() != Pk::SolidPattern) m_supported = false;
        return " " + kind + "=\"" + brush.color().name().PkToUtf8() + "\" " + kind +
            "-opacity=\"" + number(brush.color().alphaF()) + "\"";
    }
    void append(const std::string &element) {
        for (const auto &clip : m_state.clips) m_body += "<g clip-path=\"url(#" + clip + ")\">";
        m_body += "<g transform=\"" + matrix(m_state.transform) + "\" opacity=\"" + number(m_state.opacity) + "\">" + element + "</g>";
        for (std::size_t i = 0; i < m_state.clips.size(); ++i) m_body += "</g>";
    }
    void drawPath(const PkPainterPath &path, const PkBrush &brush, const PkPen &pen) {
        std::string element = "<path d=\"" + pathData(path) + "\" fill-rule=\"" +
            (path.fillRule() == Pk::OddEvenFill ? "evenodd" : "nonzero") + "\"" + brushAttributes(brush, "fill");
        if (pen.style() == Pk::NoPen) element += " stroke=\"none\"";
        else {
            element += brushAttributes(pen.brush(), "stroke") + " stroke-width=\"" + number(pen.widthF() == 0 ? 1 : pen.widthF()) +
                "\" stroke-linecap=\"" + (pen.capStyle() == Pk::FlatCap ? "butt" : pen.capStyle() == Pk::RoundCap ? "round" : "square") +
                "\" stroke-linejoin=\"" + (pen.joinStyle() == Pk::RoundJoin ? "round" : pen.joinStyle() == Pk::BevelJoin ? "bevel" : "miter") +
                "\" stroke-miterlimit=\"" + number(pen.miterLimit()) + "\"";
            if (pen.isCosmetic()) element += " vector-effect=\"non-scaling-stroke\"";
            const auto dashes = pen.dashPattern();
            if (!dashes.empty()) {
                element += " stroke-dasharray=\"";
                for (auto d : dashes) element += number(d * (pen.widthF() == 0 ? 1 : pen.widthF())) + " ";
                element += "\" stroke-dashoffset=\"" + number(pen.dashOffset() * (pen.widthF() == 0 ? 1 : pen.widthF())) + "\"";
            }
        }
        append(element + "/>");
    }
    void clip(const PkPainterPath &path, Pk::ClipOperation op) {
        if (op != Pk::IntersectClip) m_state.clips.clear();
        if (op == Pk::NoClip) return;
        const std::string id = "clip" + std::to_string(++m_id);
        m_defs += "<clipPath id=\"" + id + "\"><path d=\"" + pathData(m_state.transform.map(path)) + "\"/></clipPath>";
        m_state.clips.push_back(id);
    }
public:
    explicit PkSvgPainterBackend(const PkRectF &bounds = PkRectF()) : m_bounds(bounds) {}
    std::string document() const {
        if (!m_supported) return {};
        std::string root = "<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\"";
        if (m_bounds.isValid()) {
            root += " width=\"" + number(m_bounds.width()) + "\" height=\"" + number(m_bounds.height()) + "\" viewBox=\"" +
                number(m_bounds.x()) + " " + number(m_bounds.y()) + " " + number(m_bounds.width()) + " " + number(m_bounds.height()) + "\"";
        }
        return root + "><defs>" + m_defs + "</defs>" + m_body + "</svg>";
    }
    void submit(const PkPaintCommand &command) override {
        std::visit([this](const auto &c) {
            using T = std::decay_t<decltype(c)>;
            if constexpr (std::is_same_v<T, PkSaveCommand>) m_stack.push_back(m_state);
            else if constexpr (std::is_same_v<T, PkRestoreCommand>) { if (!m_stack.empty()) { m_state = m_stack.back(); m_stack.pop_back(); } }
            else if constexpr (std::is_same_v<T, PkSetPenCommand>) m_state.pen = c.pen;
            else if constexpr (std::is_same_v<T, PkSetBrushCommand>) m_state.brush = c.brush;
            else if constexpr (std::is_same_v<T, PkSetOpacityCommand>) m_state.opacity = c.opacity;
            else if constexpr (std::is_same_v<T, PkSetTransformCommand>) {
                m_state.transform = c.combine ? c.transform * m_state.transform : c.transform;
                if (!m_state.transform.isAffine()) m_supported = false;
            }
            else if constexpr (std::is_same_v<T, PkSetRenderHintCommand> || std::is_same_v<T, PkSetFontCommand>) {}
            else if constexpr (std::is_same_v<T, PkSetCompositionModeCommand>) { if (c.mode != Pk::CompositionMode_SourceOver) m_supported = false; }
            else if constexpr (std::is_same_v<T, PkDrawPathCommand>) drawPath(c.path, m_state.brush, m_state.pen);
            else if constexpr (std::is_same_v<T, PkFillPathCommand>) drawPath(c.path, c.brush, PkPen(Pk::NoPen));
            else if constexpr (std::is_same_v<T, PkStrokePathCommand>) drawPath(c.path, PkBrush(Pk::NoBrush), c.pen);
            else if constexpr (std::is_same_v<T, PkDrawLineCommand>) { PkPainterPath path; path.moveTo(c.line.p1()); path.lineTo(c.line.p2()); drawPath(path, PkBrush(Pk::NoBrush), m_state.pen); }
            else if constexpr (std::is_same_v<T, PkFillRectCommand> || std::is_same_v<T, PkDrawRectCommand>) {
                PkPainterPath path; path.addRect(c.rect);
                if constexpr (std::is_same_v<T, PkFillRectCommand>) drawPath(path, c.brush, PkPen(Pk::NoPen));
                else drawPath(path, m_state.brush, m_state.pen);
            }
            else if constexpr (std::is_same_v<T, PkSetClipRectCommand>) { PkPainterPath path; path.addRect(c.rect); clip(path, c.operation); }
            else if constexpr (std::is_same_v<T, PkSetClipPathCommand>) clip(c.path, c.operation);
            else if constexpr (std::is_same_v<T, PkDrawImageCommand>) {
                append("<image x=\"" + number(c.target.x()) + "\" y=\"" + number(c.target.y()) + "\" width=\"" +
                    number(c.target.width()) + "\" height=\"" + number(c.target.height()) + "\" preserveAspectRatio=\"none\" xlink:href=\"" +
                    ImageShapePngData::encodeDataUri(c.image).PkToUtf8() + "\"/>");
            }
            else m_supported = false;
        }, command);
    }
};
