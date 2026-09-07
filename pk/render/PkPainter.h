#pragma once
#include "PkPaintCommand.h"
#include <vector>

// PkPainter —— 命令录制式 2D 绘制门面（Qt 无关）。
//
// 工作方式：每个绘制调用转成一个 PkPaintCommand 提交给 PkPainterBackend；
// 自身只保留可查询的状态（pen/brush/transform/hints/opacity/composition/
// clipPath/font），供 save/restore 与 clipBoundingRect 等查询使用。
//
// 方法面按 libs/canvas + plugins/* + libs/image 的 QPainter**实测调用点**确定
// （S-09-g 扩锁）；不臆造未被调用的 API。
class PkPainter {
public:
    // Qt 5.15 QPainter::RenderHint spellings retained by Knife and Karbon.
    enum RenderHint {
        Antialiasing = 0x01,
        TextAntialiasing = 0x02,
        SmoothPixmapTransform = 0x04,
        HighQualityAntialiasing = 0x08
    };

    explicit PkPainter(PkPainterBackend &backend);

    qreal devicePixelRatio() const;

    // ---- 状态栈 ----
    void save(); void restore();

    // ---- 画具 ----
    PkPen pen() const; void setPen(const PkPen &); void setPen(const PkColor &, qreal width=1.0); void setPen(Pk::PenStyle);
    PkBrush brush() const; void setBrush(const PkBrush &); void setBrush(const PkColor &); void setBrush(Pk::BrushStyle);
    void setFont(const PkFont &); PkFont font() const;

    // ---- 变换（translate/scale/rotate 都折叠进 transform）----
    PkTransform transform() const; void setTransform(const PkTransform &, bool combine=false);
    void translate(qreal dx, qreal dy);
    void scale(qreal sx, qreal sy);
    void rotate(qreal degrees);

    // ---- 渲染提示 / 合成 / 不透明度 ----
    void setRenderHint(unsigned, bool enabled=true); void setRenderHints(unsigned, bool enabled=true);
    bool testRenderHint(unsigned hint) const;
    void setCompositionMode(Pk::CompositionMode mode); Pk::CompositionMode compositionMode() const;
    void setOpacity(qreal opacity); qreal opacity() const;

    // ---- 裁剪 ----
    void setClipRect(const PkRectF &, Pk::ClipOperation=Pk::ReplaceClip);
    void setClipPath(const PkPainterPath &, Pk::ClipOperation=Pk::ReplaceClip);
    bool hasClipping() const;
    PkRectF clipBoundingRect() const;

    // ---- 绘制 ----
    void drawLine(const PkLineF &); void drawLine(const PkPointF &, const PkPointF &);
    void drawRect(const PkRectF &);
    void drawEllipse(const PkRectF &); void drawEllipse(const PkPointF &, qreal, qreal);
    void drawArc(const PkRectF &, int, int);
    void drawPath(const PkPainterPath &); void drawPolygon(const PkPolygonF &);
    void drawPoint(const PkPointF &);
    void strokePath(const PkPainterPath &, const PkPen &);
    void fillRect(const PkRectF &); void fillRect(const PkRectF &, const PkBrush &);
    void fillPath(const PkPainterPath &); void fillPath(const PkPainterPath &, const PkBrush &);
    void drawImage(const PkRectF &, const PkImage &);
    void drawPixmap(const PkPointF &, const PkImage &);
    void drawPixmap(const PkRectF &target, const PkImage &, const PkRectF &source = PkRectF());
    void drawTiledPixmap(const PkRectF &, const PkImage &, const PkPointF &offset = PkPointF());
    void drawText(const PkPointF &, const PkString &);
    void drawText(const PkRectF &, const PkString &);

private:
    struct State {
        PkPen pen;
        PkBrush brush;
        PkFont font;
        PkTransform transform;
        unsigned hints = 0;
        qreal opacity = 1.0;
        Pk::CompositionMode mode = Pk::CompositionMode_SourceOver;
        PkPainterPath clipPath;
        bool hasClip = false;
    };
    PkPainterBackend &m_backend;
    State m_state;
    std::vector<State> m_stack;
};
