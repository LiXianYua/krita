/*
 * SPDX-FileCopyrightText: 2002-2010 Krita SVG parser contributors
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#pragma once
#include "PkSvgPathParser_p.h"
#include <PkXmlDocument.h>
#include <PkPainter.h>
#include <PkImageRasterBackend.h>
#include <PkImageFileDecoder.h>
#include <PkFontRasterizer.h>
#include <PkStrokeOutline.h>
#include <SvgTransformParser.h>
#include <SvgCssHelper.h>
#include <charconv>
#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <string_view>
#include <stdexcept>
#include <cstdlib>
#include <linebreak.h>
#include <graphemebreak.h>

namespace PkSvgDocument {
using Element = PkXmlElement;
using Style = std::map<std::string, std::string>;
constexpr std::size_t byteLimit = 512u * 1024u * 1024u;
inline std::string attr(const Element &e, const char *key, const char *fallback = "")
{ return e.attribute(key, fallback).PkToUtf8(); }
inline std::string trim(std::string value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    return first == std::string::npos ? "" : value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}
inline std::vector<double> numbers(std::string_view input)
{
    std::vector<double> result;
    while (!input.empty()) {
        const auto begin = input.find_first_not_of(" \t\r\n,");
        if (begin == std::string_view::npos) break;
        input.remove_prefix(begin);
        if (input.front() == '+') input.remove_prefix(1);
        double n = 0;
        const auto parsed = std::from_chars(input.data(), input.data() + input.size(), n);
        if (parsed.ec != std::errc() || !std::isfinite(n)) break;
        result.push_back(n);
        input.remove_prefix(std::size_t(parsed.ptr - input.data()));
    }
    return result;
}
inline double number(const std::string &input, double fallback = 0)
{ const auto values = numbers(input); return values.empty() ? fallback : values.front(); }
inline double length(const std::string &input, double base)
{
    const double value = number(input);
    if (input.empty()) return value;
    if (input.back() == '%') return value * base / 100;
    const auto unit = input.size() > 1 ? input.substr(input.size() - 2) : "";
    if (unit == "pt") return value * 96 / 72;
    if (unit == "pc") return value * 16;
    if (unit == "in") return value * 96;
    if (unit == "cm") return value * 96 / 2.54;
    if (unit == "mm") return value * 96 / 25.4;
    return value;
}
// The previous brush owner follows SVG Tiny's 90 dpi lengths. Percentages on
// geometry are parsed as their numeric prefix, unlike gradient coordinates.
inline double geometryLength(const std::string &input)
{
    const double value = number(input);
    const auto unit = input.size() > 1 ? input.substr(input.size() - 2) : "";
    if (unit == "pt") return value * 1.25;
    if (unit == "mm") return value * 3.543307;
    if (unit == "cm") return value * 35.43307;
    if (unit == "in") return value * 90;
    return value;
}
inline std::string prop(const Style &style, const char *key, const char *fallback = "")
{ const auto found = style.find(key); return found == style.end() ? fallback : found->second; }
inline Style style(const Element &e, Style result, const PkStringList &rules = {})
{
    result.erase("opacity"); result.erase("display");
    for (const char *key : {"fill", "fill-opacity", "fill-rule", "stroke", "stroke-opacity", "stroke-width",
         "stroke-linecap", "stroke-linejoin", "stroke-miterlimit", "stroke-dasharray", "stroke-dashoffset",
         "opacity", "display", "visibility", "color", "font-family", "font-size", "font-style", "font-weight",
         "text-anchor", "stop-color", "stop-opacity"})
        if (e.hasAttribute(key) && attr(e, key) != "inherit") result[key] = attr(e, key);
    const auto inlineStyle = rules.isEmpty() ? attr(e, "style") : rules.join(";").PkToUtf8();
    std::size_t start = 0;
    while (start < inlineStyle.size()) {
        const auto end = inlineStyle.find(';', start);
        const auto declaration = inlineStyle.substr(start, end == std::string::npos ? end : end - start);
        const auto colon = declaration.find(':');
        if (colon != std::string::npos) result[trim(declaration.substr(0, colon))] = trim(declaration.substr(colon + 1));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return result;
}
inline PkColor color(std::string value, const Style &style)
{
    if (value == "currentColor") value = prop(style, "color", "black");
    if (value.compare(0, 4, "rgb(") == 0) {
        auto list = value.substr(4, value.size() - 5);
        const bool percent = list.find('%') != std::string::npos;
        list.erase(std::remove(list.begin(), list.end(), '%'), list.end());
        const auto values = numbers(list);
        const auto component = [percent](double v) {
            return int(std::round(std::clamp(v * (percent ? 2.55 : 1), 0.0, 255.0)));
        };
        if (values.size() == 3) return PkColor(component(values[0]), component(values[1]), component(values[2]));
    }
    return PkColor(value.c_str());
}
inline std::vector<std::uint8_t> dataUri(const std::string &uri)
{
    if (uri.compare(0, 5, "data:") != 0) return {};
    const auto comma = uri.find(',');
    if (comma == std::string::npos || uri.substr(0, comma).find(";base64") == std::string::npos) return {};
    std::vector<std::uint8_t> result;
    unsigned buffer = 0, bits = 0;
    for (std::size_t i = comma + 1; i < uri.size(); ++i) {
        const char c = uri[i];
        if (c == '=') break;
        const int value = c >= 'A' && c <= 'Z' ? c - 'A' : c >= 'a' && c <= 'z' ? c - 'a' + 26 :
            c >= '0' && c <= '9' ? c - '0' + 52 : c == '+' ? 62 : c == '/' ? 63 : -1;
        if (value < 0) continue;
        buffer = (buffer << 6) | unsigned(value); bits += 6;
        if (bits >= 8) { bits -= 8; result.push_back(std::uint8_t(buffer >> bits)); }
    }
    return result;
}
inline bool switchConditions(const Element &e, const Style &s)
{
    if (prop(s, "display") == "none" || prop(s, "visibility") == "hidden" ||
        prop(s, "visibility") == "collapse") return false;
    for (const char *name : {"requiredExtensions", "requiredFormats", "requiredFonts"})
        if (!e.attribute(name).split(',', Pk::SkipEmptyParts).isEmpty()) return false;
    static const std::set<std::string> features {
        "SVG", "SVG-static", "CoreAttribute", "Structure", "ConditionalProcessing",
        "ConditionalProcessingAttribute", "Image", "Prefetch", "Shape", "Text",
        "PaintAttribute", "OpacityAttribute", "GraphicsAttribute", "Gradient",
        "SolidColor", "XlinkAttribute", "ExternalResourcesRequiredAttribute", "Font",
        "Hyperlinking", "Extensibility"
    };
    const std::string prefix = "http://www.w3.org/Graphics/SVG/feature/1.2/#";
    for (const auto &feature : e.attribute("requiredFeatures").split(',', Pk::SkipEmptyParts)) {
        const auto value = feature.PkToUtf8();
        if (value.compare(0, prefix.size(), prefix) || !features.count(value.substr(prefix.size()))) return false;
    }
    const auto languages = e.attribute("systemLanguage").split(',', Pk::SkipEmptyParts);
    if (!languages.isEmpty()) {
        std::string language = "C";
        for (const char *key : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
            const char *value = std::getenv(key);
            if (value && *value) { language = value; break; }
        }
        language = language.substr(0, language.find_first_of("_-.@"));
        if (language == "POSIX") language = "C";
        for (const auto &candidate : languages)
            if (candidate.PkToUtf8().compare(0, language.size(), language) == 0) return true;
        return false;
    }
    return true;
}
struct Path : PkPainterPath {
    void curveTo(const PkPointF &a, const PkPointF &b, const PkPointF &c) { cubicTo(a, b, c); }
    void closeMerge() { closeSubpath(); }
};
class Renderer {
public:
    Renderer(PkPainter &painter, const PkRectF &box, const PkSizeF &device = {}, PkRectF *bounds = nullptr)
        : painter(painter), box(box), device(device), measuredBounds(bounds) {}
    void index(const Element &e, unsigned depth = 0)
    {
        if (depth > 256 || ++nodes > 1000000) throw std::length_error("SVG element limit");
        const auto id = attr(e, "id");
        if (!id.empty()) definitions.emplace(id, e);
        if (e.tagName() == "font" && !id.empty()) {
            embeddedFonts.emplace(id, e);
            for (auto n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
                if (!n.isElement()) continue;
                const auto child = n.toElement();
                const auto family = child.tagName() == "font-face" ? attr(child, "font-family") :
                    child.tagName() == "font-face-name" ? attr(child, "name") : "";
                if (!family.empty()) embeddedFonts.emplace(family, e);
            }
        }
        if (e.tagName() == "style") css.parseStylesheet(e);
        for (auto n = e.firstChild(); !n.isNull(); n = n.nextSibling())
            if (n.isElement()) index(n.toElement(), depth + 1);
    }
    void paint(const Element &e, const Style &parent = {}, unsigned depth = 0)
    {
        if (depth > 256 || ++operations > 1000000) throw std::length_error("SVG reference limit");
        const auto tag = e.tagName().PkToUtf8();
        if (tag == "defs" || tag == "clipPath" || tag == "mask" || tag == "style" || tag == "symbol" ||
            tag == "linearGradient" || tag == "radialGradient" || tag == "title" || tag == "desc") return;
        auto s = style(e, parent, css.matchStyles(e));
        if (prop(s, "display") == "none") return;
        painter.save();
        painter.setOpacity(painter.opacity() * std::clamp(number(prop(s, "opacity", "1")), 0.0, 1.0));
        const auto parentTransform = painter.transform();
        painter.setTransform(SvgTransformParser(e.attribute("transform")).transform(), true);
        applyInitialAnimation(e, s, parentTransform);
        const auto x = [&](const char *key, const char *fallback = "0") {
            return std::string_view(key) == "width" || std::string_view(key) == "height" ?
                geometryLength(attr(e, key, fallback)) : number(attr(e, key, fallback));
        };
        const auto y = x;
        if (tag == "svg" || tag == "g" || tag == "a" || tag == "switch") {
            if (tag == "svg" && depth != 0) {
                const auto raw = numbers(attr(e, "viewBox"));
                const PkRectF source = raw.size() == 4 ? PkRectF(raw[0], raw[1], raw[2], raw[3]) :
                    PkRectF(0, 0, x("width"), y("height"));
                if (source.width() > 0 && source.height() > 0 && !device.isEmpty()) {
                    painter.scale(device.width() / source.width(), device.height() / source.height());
                    painter.translate(-source.x(), -source.y());
                }
            }
            for (auto n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
                if (!n.isElement()) continue;
                if (tag == "switch" && !switchConditions(n.toElement(), style(n.toElement(), s, css.matchStyles(n.toElement())))) continue;
                paint(n.toElement(), s, depth + 1);
                if (tag == "switch") break;
            }
        } else if (tag == "use") {
            const auto ref = attr(e, "xlink:href", attr(e, "href").c_str());
            if (!ref.empty() && ref.front() == '#') {
                const auto found = definitions.find(ref.substr(1));
                if (found != definitions.end() && active.insert(ref).second) {
                    painter.translate(x("x"), y("y"));
                    paint(found->second, s, depth + 1);
                    active.erase(ref);
                }
            }
        } else if (prop(s, "visibility") != "hidden" && prop(s, "visibility") != "collapse") {
            if (tag == "image") {
                const auto reference = trim(attr(e, "xlink:href", attr(e, "href").c_str()));
                const auto bytes = dataUri(reference);
                auto image = reference.compare(0, 5, "data:") == 0 ?
                    PkImageFileDecoder::decodeForPainting(bytes.data(), bytes.size()) : PkImageFileDecoder::loadForPainting(reference);
                if (image.format() == PkImage::Format_ARGB32)
                    image = image.convertToFormat(PkImage::Format_ARGB32_Premultiplied);
                if (!image.isNull()) {
                    const PkRectF rect(x("x"), y("y"), x("width"), y("height"));
                    includeBounds(painter.transform().mapRect(rect));
                    painter.drawImage(rect, image);
                }
            } else if (tag == "text" || tag == "textArea") {
                paintText(e, s);
            } else {
                Path path;
                bool rectangle = false;
                if (tag == "rect") {
                    const PkRectF rect(x("x"), y("y"), x("width"), y("height"));
                    if (rect.width() > 0 && rect.height() > 0) {
                        const double rx = x("rx", attr(e, "ry", "0").c_str()), ry = y("ry", attr(e, "rx", "0").c_str());
                        if (rx > 0 || ry > 0) {
                            const double relativeX = std::clamp(rx * (100 / (rect.width() / 2)), 0.0, 100.0);
                            const double relativeY = std::clamp(ry * (100 / (rect.height() / 2)), 0.0, 100.0);
                            path.addRoundedRect(rect, int(relativeX), int(relativeY), Pk::RelativeSize);
                        } else { path.addRect(rect); rectangle = true; }
                    }
                } else if (tag == "circle" || tag == "ellipse") {
                    const double rx = tag == "circle" ? x("r") : x("rx"), ry = tag == "circle" ? x("r") : y("ry");
                    if (rx > 0 && ry > 0) path.addEllipse(PkRectF(x("cx") - rx, y("cy") - ry, 2 * rx, 2 * ry));
                } else if (tag == "line") {
                    path.moveTo(x("x1"), y("y1")); path.lineTo(x("x2"), y("y2"));
                } else if (tag == "polyline" || tag == "polygon") {
                    const auto points = numbers(attr(e, "points"));
                    for (std::size_t i = 0; i + 1 < points.size(); i += 2)
                        if (!i) path.moveTo(points[i], points[i + 1]); else path.lineTo(points[i], points[i + 1]);
                    if (tag == "polygon") path.closeSubpath();
                } else if (tag == "path") {
                    PkSvgPathParser<Path> loader(&path, PkSvgArcPolicy::QtSvg);
                    loader.parseSvg(e.attribute("d"), true);
                }
                path.setFillRule(prop(s, "fill-rule") == "evenodd" ? Pk::OddEvenFill : Pk::WindingFill);
                draw(path, s, rectangle);
            }
        }
        painter.restore();
    }
private:
    void applyInitialAnimation(const Element &e, Style &s, const PkTransform &parentTransform)
    {
        // A brush owns a freshly loaded document and renders it once: preserve
        // the animation's initial frame, without introducing a timer/service.
        const auto clock = [](std::string value) {
            const auto colon = value.find(':');
            if (colon != std::string::npos) {
                double seconds = 0;
                for (;;) {
                    const auto next = value.find(':');
                    seconds = seconds * 60 + number(value.substr(0, next));
                    if (next == std::string::npos) break;
                    value.erase(0, next + 1);
                }
                return seconds * 1000;
            }
            const double scale = value.size() >= 2 && value.substr(value.size() - 2) == "ms" ? 1 :
                value.size() >= 3 && value.substr(value.size() - 3) == "min" ? 60000 :
                !value.empty() && value.back() == 'h' ? 3600000 : 1000;
            return number(value) * scale;
        };
        for (auto n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
            if (!n.isElement()) continue;
            const auto a = n.toElement();
            const auto tag = a.tagName();
            if (tag != "animateColor" && tag != "animateTransform") continue;
            const double begin = clock(attr(a, "begin")), duration = clock(attr(a, "dur"));
            if (begin > 0 || !(duration > 0)) continue;
            double progress = -begin / duration;
            const auto repeat = attr(a, "repeatCount", "1");
            if (repeat != "indefinite" && progress > number(repeat, 1)) {
                if (attr(a, "fill") != "freeze") continue;
                progress = number(repeat, 1);
            }
            if (progress > 1) progress -= std::trunc(progress);
            std::vector<std::string> values;
            auto list = attr(a, "values");
            if (!list.empty()) {
                for (;;) {
                    const auto end = list.find(';');
                    values.push_back(trim(list.substr(0, end)));
                    if (end == std::string::npos) break;
                    list.erase(0, end + 1);
                }
            } else {
                values = {attr(a, "from", "0 0 0"), attr(a, "to", attr(a, "by").c_str())};
            }
            if (values.size() < 2) continue;
            const double position = progress * (values.size() - 1);
            const auto end = std::min(values.size() - 1, std::size_t(std::max(0.0, std::ceil(position))));
            if (tag == "animateColor") {
                const auto start = std::min(values.size() - 1, std::size_t(std::max(0.0, std::floor(position))));
                const auto first = color(values[start], s), last = color(values[end], s);
                const double fraction = position > 1 ? position - std::trunc(position) : position;
                const auto channel = [fraction](int x, int y) { return int(x + (y - x) * fraction); };
                char encoded[10];
                std::snprintf(encoded, sizeof(encoded), "#%02x%02x%02x", channel(first.red(), last.red()),
                              channel(first.green(), last.green()), channel(first.blue(), last.blue()));
                const auto target = attr(a, "attributeName") == "fill" ? "fill" : "stroke";
                s[target] = encoded;
            } else {
                const auto start = end ? end - 1 : 0;
                auto first = numbers(values[start]), last = numbers(values[end]);
                first.resize(3); last.resize(3);
                if (attr(a, "to").empty() && !attr(a, "by").empty() && a.hasAttribute("from"))
                    for (int i = 0; i < 3; ++i) last[i] += first[i];
                double v[3];
                for (int i = 0; i < 3; ++i) v[i] = first[i] + (last[i] - first[i]) * progress;
                PkTransform transform;
                const auto type = attr(a, "type");
                if (type == "translate") transform.translate(v[0], v[1]);
                else if (type == "scale") transform.scale(v[0], v[1] == 0 ? v[0] : v[1]);
                else if (type == "rotate") {
                    transform.translate(v[1], v[2]); transform.rotate((last[0] - first[0]) * progress); transform.translate(-v[1], -v[2]);
                } else if (type == "skewX") transform.shear(std::tan(v[0] * (3.14159265358979323846 / 180)), 0);
                else if (type == "skewY") transform.shear(0, std::tan(v[0] * (3.14159265358979323846 / 180)));
                else continue;
                if (attr(a, "additive") != "sum" && (a.hasAttribute("from") || !attr(a, "values").empty()))
                    painter.setTransform(parentTransform);
                painter.setTransform(transform, true);
            }
        }
    }
    struct TextRun { PkPainterPath path; PkString text; PkFont font; Style style; double advance = 0; };
    void textRuns(const Element &e, const Style &s, double fontScale, std::vector<TextRun> &runs,
                  bool &appendSpace, unsigned depth = 0, bool preserve = false)
    {
        if (depth > 256) throw std::length_error("SVG text depth");
        if (e.hasAttribute("xml:space")) preserve = attr(e, "xml:space") == "preserve";
        for (auto n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
            if (n.isElement() && n.toElement().tagName() == "tspan") {
                const auto child = n.toElement();
                textRuns(child, style(child, s, css.matchStyles(child)), fontScale, runs, appendSpace, depth + 1, preserve);
            } else if (n.isElement() && n.toElement().tagName() == "tbreak" && e.tagName() == "textArea") {
                TextRun linebreak;
                linebreak.text = "\n";
                linebreak.style = s;
                linebreak.font.setPixelSize(100);
                linebreak.font.setFamily(prop(s, "font-family", "sans-serif"));
                runs.push_back(std::move(linebreak));
                appendSpace = false;
            } else if (n.isText() || n.isCDATASection()) {
                PkString text = n.isCDATASection() ? n.toCDATASection().data() : n.toText().data();
                const auto space = attr(e, "xml:space");
                if (!space.empty()) preserve = space == "preserve";
                text.replace('\t', ' '); text.replace('\n', ' ');
                const bool span = e.tagName() == "tspan";
                const bool prepend = !appendSpace && !span && !preserve && !runs.empty() && text.startsWith(' ');
                if (appendSpace && !runs.empty()) runs.back().text += ' ';
                appendSpace = !span && !preserve && text.endsWith(' ');
                if (!preserve) text = text.simplified();
                if (text.isEmpty()) appendSpace = false;
                if (prepend) text = PkString(" ") + text;
                if (text.isEmpty()) continue;
                PkFont font;
                font.setFamily(prop(s, "font-family", "sans-serif"));
                const double size = length(prop(s, "font-size", "12"), box.height()) * fontScale;
                if (!std::isfinite(size) || size <= 0 || size > 1000000) throw std::length_error("SVG font size");
                font.setPixelSize(std::max(1, int(size)));
                font.setWeight(prop(s, "font-weight") == "bold" || number(prop(s, "font-weight"), 400) >= 700 ? 75 : 50);
                if (prop(s, "font-style") == "italic") font.setItalic(true);
                else if (prop(s, "font-style") == "oblique") font.setStyle(PkFontStyleOblique);
                TextRun run;
                run.style = s;
                run.text = text;
                run.font = font;
                runs.push_back(std::move(run));
            }
        }
    }
    void paintText(const Element &e, const Style &s)
    {
        const double fontSize = length(prop(s, "font-size", "12"), box.height());
        if (!std::isfinite(fontSize) || fontSize <= 0 || fontSize > 1000000) throw std::length_error("SVG font size");
        const double scale = 100.0 / fontSize;
        std::vector<TextRun> runs;
        bool appendSpace = false;
        textRuns(e, s, scale, runs, appendSpace);
        if (paintEmbeddedText(e, s, runs, fontSize)) return;
        if (e.tagName() == "textArea") { paintTextArea(e, s, runs, scale); return; }
        PkFontRasterizer::Metrics metrics;
        for (auto &run : runs) run.path = PkFontRasterizer::outline(run.text, run.font, &run.advance, &metrics);
        double width = 0;
        for (const auto &run : runs) width += run.advance;
        width += metrics.rightOverhang;
        painter.scale(1 / scale, 1 / scale);
        // QTextLayout hands its baseline origin to the glyph engine in 26.6
        // coordinates before the SVG transform, including fractional x/y.
        painter.translate(std::trunc(length(attr(e, "x"), box.width()) * scale * 64) / 64,
                          std::trunc(length(attr(e, "y"), box.height()) * scale * 64) / 64);
        if (prop(s, "text-anchor") == "middle") painter.translate(-width / 2, 0);
        else if (prop(s, "text-anchor") == "end") painter.translate(-width, 0);
        for (const auto &run : runs) {
            draw(run.path, run.style);
            painter.translate(run.advance, 0);
        }
    }
    void paintTextArea(const Element &e, const Style &s, const std::vector<TextRun> &runs, double scale)
    {
        std::u16string text;
        std::vector<std::pair<std::size_t, const TextRun *>> spans;
        for (const auto &run : runs) {
            spans.emplace_back(text.size(), &run);
            text += run.text.PkToU16();
        }
        if (text.empty()) return;
        std::vector<char> breaks(text.size()), graphemes(text.size());
        set_linebreaks_utf16(reinterpret_cast<const utf16_t *>(text.data()), text.size(), nullptr, breaks.data());
        set_graphemebreaks_utf16(reinterpret_cast<const utf16_t *>(text.data()), text.size(), nullptr, graphemes.data());
        const double maxWidth = number(attr(e, "width")) * scale;
        const double maxHeight = number(attr(e, "height")) * scale;
        struct Line { std::vector<TextRun> runs; double width = 0, ascent = 0, descent = 0; };
        const auto layout = [&](std::size_t begin, std::size_t end) {
            while (end > begin && (text[end - 1] == u' ' || text[end - 1] == u'\n')) --end;
            Line line;
            for (std::size_t i = 0; i < spans.size(); ++i) {
                const auto start = std::max(begin, spans[i].first);
                const auto stop = std::min(end, i + 1 == spans.size() ? text.size() : spans[i + 1].first);
                if (start >= stop) continue;
                TextRun run = *spans[i].second;
                run.text = PkString::fromUtf16(text.data() + start, int(stop - start));
                PkFontRasterizer::Metrics metrics;
                run.path = PkFontRasterizer::outline(run.text, run.font, &run.advance, &metrics);
                line.width += run.advance;
                line.ascent = std::max(line.ascent, metrics.ascent);
                line.descent = std::max(line.descent, metrics.descent);
                line.runs.push_back(std::move(run));
            }
            if (line.runs.empty()) {
                const TextRun *run = spans.front().second;
                for (const auto &span : spans) if (span.first <= begin) run = span.second;
                PkFontRasterizer::Metrics metrics;
                PkFontRasterizer::outline(" ", run->font, nullptr, &metrics);
                line.ascent = metrics.ascent; line.descent = metrics.descent;
            }
            return line;
        };
        painter.scale(1 / scale, 1 / scale);
        double px = number(attr(e, "x")) * scale;
        if (prop(s, "text-anchor") == "middle") px += maxWidth / 2;
        else if (prop(s, "text-anchor") == "end") px += maxWidth;
        const double py = number(attr(e, "y")) * scale;
        double y = 0;
        for (std::size_t begin = 0; begin < text.size();) {
            Line selected;
            std::size_t end = begin;
            for (std::size_t candidate = begin + 1; candidate <= text.size(); ++candidate) {
                if (candidate != text.size() && breaks[candidate - 1] != LINEBREAK_ALLOWBREAK &&
                    breaks[candidate - 1] != LINEBREAK_MUSTBREAK) continue;
                auto line = layout(begin, candidate);
                if (maxWidth > 0 && line.width > maxWidth) break;
                selected = std::move(line); end = candidate;
                if (breaks[candidate - 1] == LINEBREAK_MUSTBREAK) break;
            }
            if (end == begin) {
                for (std::size_t candidate = begin + 1; candidate <= text.size(); ++candidate) {
                    if (candidate != text.size() && graphemes[candidate - 1] != GRAPHEMEBREAK_BREAK) continue;
                    auto line = layout(begin, candidate);
                    if (end != begin && maxWidth > 0 && line.width > maxWidth) break;
                    selected = std::move(line); end = candidate;
                    if (maxWidth > 0 && selected.width > maxWidth) break;
                }
            }
            if ((maxWidth > 0 && selected.width > maxWidth) ||
                (maxHeight > 0 && y + selected.ascent + selected.descent > maxHeight)) break;
            double x = 0;
            if (prop(s, "text-anchor") == "middle") x = -selected.width / 2;
            else if (prop(s, "text-anchor") == "end") x = -selected.width;
            painter.save();
            // Line positions and the final baseline are separate 26.6 values.
            const double lineX = std::trunc(x * 64) / 64, lineY = std::trunc(y * 64) / 64;
            painter.translate(std::trunc((px + lineX) * 64) / 64,
                              std::trunc((py + lineY + selected.ascent) * 64) / 64);
            for (const auto &run : selected.runs) {
                draw(run.path, run.style);
                painter.translate(run.advance, 0);
            }
            painter.restore();
            y += 1.1 * (selected.ascent + selected.descent);
            begin = end;
        }
    }
    bool paintEmbeddedText(const Element &e, const Style &s, const std::vector<TextRun> &runs, double fontSize)
    {
        const auto found = embeddedFonts.find(prop(s, "font-family"));
        if (found == embeddedFonts.end()) return false;
        const auto &font = found->second;
        double em = 1000;
        struct Glyph { Path path; double advance; };
        std::map<char16_t, Glyph> glyphs;
        for (auto n = font.firstChild(); !n.isNull(); n = n.nextSibling()) {
            if (!n.isElement()) continue;
            const auto child = n.toElement();
            if (child.tagName() == "font-face") {
                em = number(attr(child, "units-per-em", "1000"));
                if (!em) em = 1000;
            } else if (child.tagName() == "glyph" || child.tagName() == "missing-glyph") {
                const auto unicode = child.attribute("unicode").PkToU16();
                Glyph glyph;
                glyph.advance = number(attr(child, "horiz-adv-x", attr(font, "horiz-adv-x", "0").c_str()));
                PkSvgPathParser<Path> parser(&glyph.path, PkSvgArcPolicy::QtSvg);
                parser.parseSvg(child.attribute("d"), true);
                glyph.path.setFillRule(Pk::WindingFill);
                glyphs[unicode.empty() ? 0 : unicode.front()] = std::move(glyph);
            }
        }
        PkString text;
        for (const auto &run : runs) text += run.text;
        const auto characters = text.PkToU16();
        double width = 0;
        const auto glyphFor = [&](char16_t character) {
            const auto found = glyphs.find(character);
            return found == glyphs.end() ? glyphs.find(0) : found;
        };
        for (char16_t character : characters) {
            const auto glyph = glyphFor(character);
            if (glyph != glyphs.end()) width += std::trunc(glyph->second.advance);
        }
        painter.translate(number(attr(e, "x")), number(attr(e, "y")));
        painter.scale(fontSize / em, -fontSize / em);
        if (prop(s, "text-anchor") == "middle") painter.translate(-std::trunc(width / 2), 0);
        else if (prop(s, "text-anchor") == "end") painter.translate(-width, 0);
        auto glyphStyle = s;
        glyphStyle["stroke-width"] = std::to_string(number(prop(s, "stroke-width", "1")) * em / fontSize);
        for (char16_t character : characters) {
            const auto glyph = glyphFor(character);
            if (glyph == glyphs.end()) continue;
            draw(glyph->second.path, glyphStyle);
            painter.translate(glyph->second.advance, 0);
        }
        return true;
    }
    PkBrush brush(const std::string &value, const Style &s, unsigned depth = 0)
    {
        if (value == "none") return PkBrush(Pk::NoBrush);
        if (value.compare(0, 5, "url(#")) return PkBrush(color(value, s));
        if (depth > 256) return PkBrush(Pk::NoBrush);
        const auto found = definitions.find(value.substr(5, value.find(')') - 5));
        if (found == definitions.end()) return PkBrush(Pk::NoBrush);
        const auto &e = found->second;
        if (e.tagName() == "solidColor") {
            auto result = color(attr(e, "solid-color"), s);
            result.setAlphaF(std::clamp(number(attr(e, "solid-opacity", attr(e, "opacity", "1").c_str())), 0.0, 1.0));
            return PkBrush(result);
        }
        if (e.tagName() != "linearGradient" && e.tagName() != "radialGradient") {
            return PkBrush(Pk::NoBrush);
        }
        const bool object = attr(e, "gradientUnits") != "userSpaceOnUse";
        const auto x = [&](const char *key, const char *fallback) { return length(attr(e, key, fallback), 1); };
        const auto y = x;
        PkGradient gradient = e.tagName() == "linearGradient" ?
            PkGradient::linear(PkPointF(x("x1", "0%"), y("y1", "0%")), PkPointF(x("x2", "100%"), y("y2", "0%"))) :
            PkGradient::radial(PkPointF(x("cx", "50%"), y("cy", "50%")), x("r", "50%"),
                PkPointF(x("fx", attr(e, "cx", "50%").c_str()), y("fy", attr(e, "cy", "50%").c_str())));
        gradient.setCoordinateMode(object ? PkGradient::ObjectBoundingMode : PkGradient::LogicalMode);
        gradient.setInterpolationMode(PkGradient::ComponentInterpolation);
        gradient.setSpread(attr(e, "spreadMethod") == "reflect" ? PkGradient::ReflectSpread :
            attr(e, "spreadMethod") == "repeat" ? PkGradient::RepeatSpread : PkGradient::PadSpread);
        double offset = 0;
        bool hasStops = false;
        for (auto n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
            if (!n.isElement() || n.toElement().tagName() != "stop") continue;
            const auto stop = n.toElement();
            const auto ss = style(stop, style(e, s, css.matchStyles(e)), css.matchStyles(stop));
            auto c = color(prop(ss, "stop-color", "black"), ss);
            c.setAlphaF(std::clamp(number(prop(ss, "stop-opacity", "1")), 0.0, 1.0));
            double nextOffset = std::clamp(length(attr(stop, "offset"), 1), 0.0, 1.0);
            if (hasStops && nextOffset <= offset) nextOffset = offset + std::numeric_limits<float>::epsilon();
            if (nextOffset > 1) {
                auto stops = gradient.stops();
                if (stops.size() == 1 || stops[stops.size() - 2].offset < 1 - std::numeric_limits<float>::epsilon()) {
                    stops.last().offset = 1 - std::numeric_limits<float>::epsilon();
                    gradient.setStops(stops);
                }
                nextOffset = 1;
            }
            offset = nextOffset;
            gradient.setColorAt(offset, c); hasStops = true;
        }
        PkTransform transform;
        const auto ref = attr(e, "xlink:href", attr(e, "href").c_str());
        if (!ref.empty() && ref.front() == '#') {
            const auto inherited = brush("url(" + ref + ")", s, depth + 1);
            if (inherited.gradient()) {
                if (!hasStops) gradient.setStops(inherited.gradient()->stops());
                transform = inherited.transform();
            }
        }
        PkBrush result(gradient);
        if (e.hasAttribute("gradientTransform")) transform = SvgTransformParser(e.attribute("gradientTransform")).transform();
        result.setTransform(transform);
        return result;
    }
    void draw(const PkPainterPath &path, const Style &s, bool rectangle = false)
    {
        const double opacity = painter.opacity();
        painter.setOpacity(opacity * std::clamp(number(prop(s, "fill-opacity", "1")), 0.0, 1.0));
        const auto fill = brush(prop(s, "fill", "black"), s);
        if (fill.style() != Pk::NoBrush && !path.isEmpty()) includeBounds(painter.transform().map(path).boundingRect());
        if (rectangle) {
            painter.setPen(PkPen(Pk::NoPen));
            painter.setBrush(fill);
            painter.drawRect(path.boundingRect());
        } else painter.fillPath(path, fill);
        const auto stroke = brush(prop(s, "stroke", "none"), s);
        const double width = length(prop(s, "stroke-width", "1"), std::hypot(box.width(), box.height()) / std::sqrt(2.0));
        if (stroke.style() != Pk::NoBrush && width > 0) {
            PkPen pen(stroke, width);
            pen.setCapStyle(prop(s, "stroke-linecap") == "round" ? Pk::RoundCap : prop(s, "stroke-linecap") == "square" ? Pk::SquareCap : Pk::FlatCap);
            pen.setJoinStyle(prop(s, "stroke-linejoin") == "round" ? Pk::RoundJoin : prop(s, "stroke-linejoin") == "bevel" ? Pk::BevelJoin : Pk::SvgMiterJoin);
            pen.setMiterLimit(number(prop(s, "stroke-miterlimit", "4")));
            auto dashes = numbers(prop(s, "stroke-dasharray"));
            for (auto &dash : dashes) dash /= width;
            if (!dashes.empty()) pen.setDashPattern(dashes);
            pen.setDashOffset(length(prop(s, "stroke-dashoffset"), width) / width);
            if (measuredBounds) includeBounds(painter.transform().map(PkRender::createStrokeOutline(path, pen)).boundingRect());
            painter.setOpacity(opacity * std::clamp(number(prop(s, "stroke-opacity", "1")), 0.0, 1.0));
            painter.strokePath(path, pen);
        }
        painter.setOpacity(opacity);
    }
    void includeBounds(const PkRectF &rect)
    {
        if (!measuredBounds) return;
        *measuredBounds = hasBounds ? measuredBounds->united(rect) : rect;
        hasBounds = true;
    }
    PkPainter &painter;
    PkRectF box;
    PkSizeF device;
    PkRectF *measuredBounds = nullptr;
    bool hasBounds = false;
    SvgCssHelper css;
    std::map<std::string, Element> definitions;
    std::map<std::string, Element> embeddedFonts;
    std::set<std::string> active;
    std::size_t nodes = 0, operations = 0;
};
}

inline PkImage pkRenderSvgDocument(const char *data, std::size_t size, int width)
{
    using namespace PkSvgDocument;
    if (!data || !size || size > byteLimit || width <= 0 || width > 32767) return {};
    PkXmlDocument xml;
    if (!xml.setContentPreservingWhitespace(PkString::PkFromUtf8(data, int(size)))) return {};
    const auto root = xml.documentElement();
    if (root.tagName() != "svg") return {};
    const auto raw = numbers(attr(root, "viewBox"));
    PkRectF box = raw.size() == 4 ? PkRectF(raw[0], raw[1], raw[2], raw[3]) :
        PkRectF(0, 0, geometryLength(attr(root, "width")), geometryLength(attr(root, "height")));
    if (raw.empty() && (box.width() == 0 || box.height() == 0)) {
        struct BoundsBackend final : PkPainterBackend { void submit(const PkPaintCommand &) override {} } backend;
        PkPainter painter(backend);
        Renderer renderer(painter, box, {}, &box);
        renderer.index(root);
        renderer.paint(root);
    }
    if (!(box.width() > 0) || !(box.height() > 0)) return {};
    for (double value : {box.x(), box.y(), box.width(), box.height()})
        if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<int>::max() / 2.0) return {};
    // KisSvgBrush historically sizes from the integer viewBox(), while the
    // renderer maps the floating viewBoxF(). Preserve both boundaries.
    const auto integerBox = box.toRect();
    if (integerBox.width() <= 0 || integerBox.height() <= 0) return {};
    const double scaledHeight = double(width) * integerBox.height() / integerBox.width();
    if (!std::isfinite(scaledHeight) || scaledHeight > 32767) return {};
    const int height = std::max(1, int(scaledHeight));
    if (std::size_t(height) > byteLimit / (std::size_t(width) * 4)) return {};
    PkImage image(width, height, PkImage::Format_ARGB32);
    if (image.isNull()) return {};
    image.fill(0xffffffffu);
    PkImageRasterBackend backend(image);
    PkPainter painter(backend);
    painter.setRenderHint(PkPainter::Antialiasing);
    painter.setRenderHint(PkPainter::SmoothPixmapTransform);
    painter.scale(width / box.width(), height / box.height());
    painter.translate(-box.x(), -box.y());
    Renderer renderer(painter, box, PkSizeF(width, height));
    renderer.index(root);
    renderer.paint(root);
    return image;
}
