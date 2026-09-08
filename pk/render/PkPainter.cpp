#include "PkPainter.h"
#include "PkClipRegion.h"
#include <algorithm>
#include <cmath>

namespace {
// The geometry owner's current bounds include control points. Painter clip
// queries require the actual cubic extrema before mapping the conservative
// box into device space (Qt 5.15 qpainterpath.cpp).
PkRectF clipPathBounds(const PkPainterPath &path)
{
    if (!path.elementCount()) return PkRectF();
    auto first = path.elementAt(0);
    qreal minx = first.x, maxx = first.x, miny = first.y, maxy = first.y;
    const auto include = [&](qreal x, qreal y) {
        minx = std::min(minx, x); maxx = std::max(maxx, x);
        miny = std::min(miny, y); maxy = std::max(maxy, y);
    };
    for (int i = 1; i < path.elementCount(); ++i) {
        const auto e = path.elementAt(i);
        if (e.type != PkPainterPath::CurveToElement) {
            include(e.x, e.y);
            continue;
        }
        const auto p = path.elementAt(i - 1), q = path.elementAt(i + 1), r = path.elementAt(i + 2);
        include(r.x, r.y);
        const auto at = [&](qreal t) {
            if (t < 0 || t > 1) return;
            const qreal u = 1 - t;
            include(u*u*u*p.x + 3*t*u*u*e.x + 3*t*t*u*q.x + t*t*t*r.x,
                    u*u*u*p.y + 3*t*u*u*e.y + 3*t*t*u*q.y + t*t*t*r.y);
        };
        const auto extrema = [&](qreal p0, qreal p1, qreal p2, qreal p3) {
            const qreal a = 3 * (-p0 + 3*p1 - 3*p2 + p3);
            const qreal b = 6 * (p0 - 2*p1 + p2);
            const qreal c = 3 * (-p0 + p1);
            if (std::abs(a) <= 1e-12) {
                if (std::abs(b) > 1e-12) at(-c / b);
            } else {
                const qreal discriminant = b*b - 4*a*c;
                if (discriminant >= 0) {
                    const qreal root = std::sqrt(discriminant), reciprocal = 1 / (2*a);
                    at((-b + root) * reciprocal);
                    at((-b - root) * reciprocal);
                }
            }
        };
        extrema(p.x, e.x, q.x, r.x);
        extrema(p.y, e.y, q.y, r.y);
        i += 2;
    }
    return PkRectF(minx, miny, maxx - minx, maxy - miny);
}
}

PkPainter::PkPainter(PkPainterBackend &b) : m_backend(b) {}

qreal PkPainter::devicePixelRatio() const { return m_backend.devicePixelRatio(); }

void PkPainter::save() { m_stack.push_back(m_state); m_backend.submit(PkSaveCommand{}); }
void PkPainter::restore() { if (m_stack.empty()) return; m_state=m_stack.back(); m_stack.pop_back(); m_backend.submit(PkRestoreCommand{}); }

PkPen PkPainter::pen() const { return m_state.pen; }
void PkPainter::setPen(const PkPen &p) { m_state.pen=p; m_backend.submit(PkSetPenCommand{p}); }
void PkPainter::setPen(const PkColor &c,qreal w) { setPen(PkPen(c,w)); }
void PkPainter::setPen(Pk::PenStyle s) { PkPen p; p.setStyle(s); setPen(p); }

PkBrush PkPainter::brush() const { return m_state.brush; }
void PkPainter::setBrush(const PkBrush &b) { m_state.brush=b; m_backend.submit(PkSetBrushCommand{b}); }
void PkPainter::setBrush(const PkColor &c) { setBrush(PkBrush(c)); }
void PkPainter::setBrush(Pk::BrushStyle s) { setBrush(PkBrush(s)); }

void PkPainter::setFont(const PkFont &f) { m_state.font=f; m_backend.submit(PkSetFontCommand{f}); }
PkFont PkPainter::font() const { return m_state.font; }

PkTransform PkPainter::transform() const { return m_state.transform; }
void PkPainter::setTransform(const PkTransform &t,bool combine) {
    m_state.transform = combine ? t*m_state.transform : t;
    m_backend.submit(PkSetTransformCommand{t,combine});
}
void PkPainter::translate(qreal dx,qreal dy) {
    PkTransform t; t.translate(dx,dy);
    m_state.transform = t*m_state.transform;
    m_backend.submit(PkSetTransformCommand{t,true});
}
void PkPainter::scale(qreal sx,qreal sy) {
    PkTransform t; t.scale(sx,sy);
    m_state.transform = t*m_state.transform;
    m_backend.submit(PkSetTransformCommand{t,true});
}
void PkPainter::rotate(qreal degrees) {
    PkTransform t; t.rotate(degrees);
    m_state.transform = t*m_state.transform;
    m_backend.submit(PkSetTransformCommand{t,true});
}

void PkPainter::setRenderHint(unsigned h,bool e) {
    if(e) m_state.hints|=h; else m_state.hints&=~h;
    m_backend.submit(PkSetRenderHintCommand{h,e});
}
void PkPainter::setRenderHints(unsigned h,bool e) { setRenderHint(h,e); }
bool PkPainter::testRenderHint(unsigned hint) const { return (m_state.hints & hint) != 0; }

void PkPainter::setCompositionMode(Pk::CompositionMode mode) {
    m_state.mode = mode;
    m_backend.submit(PkSetCompositionModeCommand{mode});
}
Pk::CompositionMode PkPainter::compositionMode() const { return m_state.mode; }

void PkPainter::setOpacity(qreal opacity) {
    m_state.opacity = opacity;
    m_backend.submit(PkSetOpacityCommand{opacity});
}
qreal PkPainter::opacity() const { return m_state.opacity; }

void PkPainter::setClipRect(const PkRectF &r,Pk::ClipOperation o) {
    const auto requestedOperation = o;
    if (!m_state.hasClip && o != Pk::NoClip) o = Pk::ReplaceClip;
    if (o == Pk::NoClip || o == Pk::ReplaceClip) m_state.clips.clear();
    m_state.clips.push_back({PkPainterPath(), r, m_state.transform, o, true});
    m_state.hasClip = o != Pk::NoClip;
    m_backend.submit(PkSetClipRectCommand{r,requestedOperation});
}
void PkPainter::setClipPath(const PkPainterPath &p,Pk::ClipOperation o) {
    const auto requestedOperation = o;
    if (!m_state.hasClip && o != Pk::NoClip) o = Pk::ReplaceClip;
    if (o == Pk::NoClip || o == Pk::ReplaceClip) m_state.clips.clear();
    m_state.clips.push_back({p, PkRectF(), m_state.transform, o, false});
    m_state.hasClip = o != Pk::NoClip;
    m_backend.submit(PkSetClipPathCommand{p,requestedOperation});
}
bool PkPainter::hasClipping() const { return m_state.hasClip; }

PkPainterPath PkPainter::clipPath() const
{
    PkPainterPath result;
    if (m_state.clips.empty()) return result;
    const auto inverse = m_state.transform.inverted();
    if (m_state.clips.size() == 1 && !m_state.clips.front().rectangle) {
        const auto &clip = m_state.clips.front();
        return (clip.transform * inverse).map(clip.path);
    }
    PkRegion region;
    bool lastWasNothing = true;
    for (const auto &clip : m_state.clips) {
        const auto matrix = clip.transform * inverse;
        PkRegion next;
        if (clip.rectangle && matrix.type() <= PkTransform::TxScale) {
            const auto rect = clip.rect.toRect();
            if (matrix.type() <= PkTransform::TxTranslate) {
                next = PkRegion(rect).translated(int(std::round(matrix.dx())), int(std::round(matrix.dy())));
            } else {
                next = PkRegion(matrix.mapRect(PkRectF(rect)).toRect());
            }
        } else {
            PkPainterPath path = clip.path;
            if (clip.rectangle) path.addRect(PkRectF(clip.rect.toRect()));
            next = PkRender::clipRegionFromPolygon(matrix.map(path).toFillPolygon(PkTransform()).toPolygon(), path.fillRule());
        }
        if (lastWasNothing) { region = next; lastWasNothing = false; }
        else if (clip.operation == Pk::IntersectClip) {
            // Intersect the two unions of rectangles pairwise. Sequentially
            // intersecting with each disjoint right-hand rectangle erases
            // valid coverage (the current geometry region operator does that).
            PkRegion intersection;
            for (const auto &left : region) for (const auto &right : next)
                intersection |= left.intersected(right);
            region = std::move(intersection);
        }
        else if (clip.operation == Pk::NoClip) { region = PkRegion(); lastWasNothing = true; }
        else region = next;
    }
    // Non-overlapping region rectangles all have the same winding. Shared
    // edges cancel during fill, preserving the region's exact covered area.
    result.setFillRule(Pk::WindingFill);
    for (const auto &rect : region) result.addRect(PkRectF(rect));
    return result;
}

PkRectF PkPainter::clipBoundingRect() const {
    // Qt accumulates conservative bounds in device space, then maps that box
    // back to the current logical coordinates. It does not intersect the
    // device bounds and returns an empty rectangle when no clip was recorded.
    PkRectF bounds;
    bool first = true;
    for (const auto &clip : m_state.clips) {
        const auto rect = clip.transform.mapRect(clip.rectangle ? clip.rect : clipPathBounds(clip.path));
        if (first) bounds = rect;
        else if (clip.operation == Pk::IntersectClip) bounds &= rect;
        first = false;
    }
    return m_state.transform.inverted().mapRect(bounds);
}

void PkPainter::drawLine(const PkLineF &l) { m_backend.submit(PkDrawLineCommand{l}); }
void PkPainter::drawLine(const PkPointF &a,const PkPointF &b) { drawLine(PkLineF(a,b)); }
void PkPainter::drawRect(const PkRectF &r) { m_backend.submit(PkDrawRectCommand{r}); }
void PkPainter::drawEllipse(const PkRectF &r) { m_backend.submit(PkDrawEllipseCommand{r}); }
void PkPainter::drawEllipse(const PkPointF &c,qreal rx,qreal ry) { drawEllipse(PkRectF(c.x()-rx,c.y()-ry,2*rx,2*ry)); }
void PkPainter::drawArc(const PkRectF &r,int s,int span) { m_backend.submit(PkDrawArcCommand{r,s,span}); }
void PkPainter::drawPath(const PkPainterPath &p) { m_backend.submit(PkDrawPathCommand{p}); }
void PkPainter::drawPolygon(const PkPolygonF &p) { m_backend.submit(PkDrawPolygonCommand{p}); }
void PkPainter::drawPoint(const PkPointF &p) { m_backend.submit(PkDrawPointCommand{p}); }
void PkPainter::strokePath(const PkPainterPath &p,const PkPen &pen) { m_backend.submit(PkStrokePathCommand{p,pen}); }

void PkPainter::fillRect(const PkRectF &r) { fillRect(r, m_state.brush); }
void PkPainter::fillRect(const PkRectF &r,const PkBrush &b) { m_backend.submit(PkFillRectCommand{r,b}); }
void PkPainter::fillPath(const PkPainterPath &p) { fillPath(p, m_state.brush); }
void PkPainter::fillPath(const PkPainterPath &p,const PkBrush &b) { m_backend.submit(PkFillPathCommand{p,b}); }
void PkPainter::fillTexturePath(const PkPainterPath &p,const PkImage &i,const PkTransform &t) { m_backend.submit(PkFillTexturePathCommand{p,i,t}); }

void PkPainter::drawImage(const PkRectF &r,const PkImage &i) { m_backend.submit(PkDrawImageCommand{r,i}); }
void PkPainter::drawPixmap(const PkPointF &pos,const PkImage &img) {
    m_backend.submit(PkDrawPixmapCommand{PkRectF(pos, PkSizeF(img.width(), img.height())), img, PkRectF()});
}
void PkPainter::drawPixmap(const PkRectF &target,const PkImage &img,const PkRectF &source) {
    m_backend.submit(PkDrawPixmapCommand{target, img, source});
}
void PkPainter::drawTiledPixmap(const PkRectF &r,const PkImage &img,const PkPointF &offset) {
    m_backend.submit(PkDrawTiledPixmapCommand{r, img, offset});
}

void PkPainter::drawText(const PkPointF &pos,const PkString &text) {
    m_backend.submit(PkDrawTextAtPointCommand{pos, text});
}
void PkPainter::drawText(const PkRectF &rect,const PkString &text) {
    m_backend.submit(PkDrawTextInRectCommand{rect, text});
}
