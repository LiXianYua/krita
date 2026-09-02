#include "TransformToolPlatform.h"

#include <KoCanvasBase.h>

#include <PkBrush.h>
#include <PkColor.h>
#include <PkImage.h>
#include <PkPainterPath.h>
#include <PkPen.h>
#include <PkRect.h>

TransformToolPainter::TransformToolPainter(PkPainter &painter)
    : m_painter(painter)
{
}

void TransformToolPainter::save()
{
    m_painter.save();
    m_opacityStack.append(m_opacityStack.last());
}

void TransformToolPainter::restore()
{
    if (m_opacityStack.size() > 1) {
        m_opacityStack.removeLast();
    }
    m_painter.restore();
}

void TransformToolPainter::setOpacity(qreal opacity)
{
    m_opacityStack.last() = qBound(qreal(0.0), opacity, qreal(1.0));
}

PkTransform TransformToolPainter::transform() const
{
    return m_painter.transform();
}

void TransformToolPainter::setTransform(const PkTransform &transform, bool combine)
{
    m_painter.setTransform(transform, combine);
}

void TransformToolPainter::setPen(const PkPen &pen)
{
    m_painter.setPen(pen);
}

void TransformToolPainter::setBrush(const PkBrush &brush)
{
    m_painter.setBrush(brush);
}

PkBrush TransformToolPainter::brush() const
{
    return m_painter.brush();
}

PkColor TransformToolPainter::withOpacity(const PkColor &color, qreal opacity)
{
    PkColor result = color;
    result.setAlphaF(result.alphaF() * opacity);
    return result;
}

PkImage TransformToolPainter::withOpacity(const PkImage &image, qreal opacity)
{
    if (opacity >= 1.0 || image.isNull()) return image;

    PkImage result = image.convertToFormat(PkImage::Format_ARGB32);
    for (int y = 0; y < result.height(); ++y) {
        for (int x = 0; x < result.width(); ++x) {
            const quint32 pixel = result.pixel(x, y);
            const quint32 alpha = qRound(((pixel >> 24) & 0xffu) * opacity);
            result.setPixel(x, y, (pixel & 0x00ffffffu) | (alpha << 24));
        }
    }
    return result;
}

template<typename Draw>
void TransformToolPainter::drawWithOpacity(Draw draw)
{
    const qreal opacity = m_opacityStack.last();
    if (opacity >= 1.0) {
        draw();
        return;
    }

    m_painter.save();
    PkPen pen = m_painter.pen();
    pen.setColor(withOpacity(pen.color(), opacity));
    m_painter.setPen(pen);
    PkBrush brush = m_painter.brush();
    brush.setColor(withOpacity(brush.color(), opacity));
    m_painter.setBrush(brush);
    draw();
    m_painter.restore();
}

void TransformToolPainter::drawImage(const PkPointF &position, const PkImage &image)
{
    const PkImage renderedImage = withOpacity(image, m_opacityStack.last());
    m_painter.drawImage(PkRectF(position.x(), position.y(),
                                renderedImage.width(), renderedImage.height()),
                        renderedImage);
}

void TransformToolPainter::drawPath(const PkPainterPath &path)
{
    drawWithOpacity([&] { m_painter.drawPath(path); });
}

void TransformToolPainter::drawLine(const PkPointF &from, const PkPointF &to)
{
    drawWithOpacity([&] { m_painter.drawLine(from, to); });
}

void TransformToolPainter::drawEllipse(const PkRectF &rect)
{
    drawWithOpacity([&] { m_painter.drawEllipse(rect); });
}

namespace {
PkColor transformHandleLineColor(TransformHandleStyle style)
{
    switch (style) {
    case TransformHandleStyle::HighlightedPrimaryHandles:
    case TransformHandleStyle::HighlightedPrimaryHandlesWithSolidOutline:
        return PkColor(155, 0, 0);
    case TransformHandleStyle::PrimarySelection:
    case TransformHandleStyle::SelectedPrimaryHandles:
        return PkColor(0, 0, 90, 180);
    }
    return PkColor(0, 0, 90, 180);
}

PkColor transformHandleFillColor(TransformHandleStyle style)
{
    switch (style) {
    case TransformHandleStyle::HighlightedPrimaryHandles:
    case TransformHandleStyle::HighlightedPrimaryHandlesWithSolidOutline:
        return PkColor(255, 100, 100);
    case TransformHandleStyle::SelectedPrimaryHandles:
        return PkColor(164, 227, 243);
    case TransformHandleStyle::PrimarySelection:
        return PkColor(Qt::white);
    }
    return PkColor(Qt::white);
}

PkPen transformHandleOutlinePen(int decorationThickness)
{
    PkPen pen(Qt::white);
    pen.setWidthF(decorationThickness);
    pen.setCosmetic(true);
    return pen;
}

PkPen transformHandleMainLinePen(TransformHandleStyle style,
                                 int decorationThickness)
{
    PkPen pen(transformHandleLineColor(style));
    pen.setWidthF(decorationThickness);
    pen.setCosmetic(true);
    pen.setJoinStyle(Qt::RoundJoin);
    if (style != TransformHandleStyle::HighlightedPrimaryHandlesWithSolidOutline) {
        pen.setDashPattern({4.0, 4.0});
    }
    return pen;
}
}

TransformToolHandlePainter::TransformToolHandlePainter(
    TransformToolPainter &painter, qreal handleRadius,
    int decorationThickness)
    : m_painter(painter)
    , m_painterTransform(painter.transform())
    , m_handleRadius(handleRadius)
    , m_decorationThickness(decorationThickness)
{
    m_painter.save();
    m_painter.setTransform(PkTransform());
}

TransformToolHandlePainter::~TransformToolHandlePainter()
{
    m_painter.restore();
}

void TransformToolHandlePainter::setHandleStyle(TransformHandleStyle style)
{
    m_style = style;
}

PkPen TransformToolHandlePainter::handlePen() const
{
    PkPen pen(transformHandleLineColor(m_style));
    pen.setWidthF(2.0 * m_decorationThickness);
    pen.setCosmetic(true);
    pen.setJoinStyle(Qt::RoundJoin);
    return pen;
}

PkBrush TransformToolHandlePainter::handleBrush() const
{
    return PkBrush(transformHandleFillColor(m_style));
}

void TransformToolHandlePainter::drawHandleCircle(const PkPointF &center)
{
    const PkPointF mappedCenter = m_painterTransform.map(center);
    const PkRectF rect(mappedCenter.x() - m_handleRadius,
                       mappedCenter.y() - m_handleRadius,
                       2.0 * m_handleRadius, 2.0 * m_handleRadius);
    m_painter.save();
    m_painter.setPen(handlePen());
    m_painter.setBrush(handleBrush());
    m_painter.drawEllipse(rect);
    m_painter.restore();
}

void TransformToolHandlePainter::drawHandleSmallCircle(const PkPointF &center)
{
    const qreal savedRadius = m_handleRadius;
    m_handleRadius *= 0.7;
    drawHandleCircle(center);
    m_handleRadius = savedRadius;
}

void TransformToolHandlePainter::drawLinePasses(const PkPointF &from,
                                                const PkPointF &to)
{
    if (m_style != TransformHandleStyle::HighlightedPrimaryHandlesWithSolidOutline) {
        m_painter.setPen(transformHandleOutlinePen(m_decorationThickness));
        m_painter.drawLine(from, to);
    }
    m_painter.setPen(transformHandleMainLinePen(m_style, m_decorationThickness));
    m_painter.drawLine(from, to);
}

void TransformToolHandlePainter::drawPathPasses(const PkPainterPath &path)
{
    if (m_style != TransformHandleStyle::HighlightedPrimaryHandlesWithSolidOutline) {
        m_painter.setPen(transformHandleOutlinePen(m_decorationThickness));
        m_painter.drawPath(path);
    }
    m_painter.setPen(transformHandleMainLinePen(m_style, m_decorationThickness));
    m_painter.drawPath(path);
}

void TransformToolHandlePainter::drawConnectionLine(const PkPointF &from,
                                                     const PkPointF &to)
{
    m_painter.save();
    m_painter.setBrush(PkBrush(Qt::NoBrush));
    drawLinePasses(m_painterTransform.map(from), m_painterTransform.map(to));
    m_painter.restore();
}

void TransformToolHandlePainter::drawPath(const PkPainterPath &path)
{
    m_painter.save();
    m_painter.setBrush(PkBrush(Qt::NoBrush));
    drawPathPasses(m_painterTransform.map(path));
    m_painter.restore();
}

namespace {
std::weak_ptr<TransformToolPlatformServices> s_services;
}

void setTransformToolPlatformServices(
    const std::shared_ptr<TransformToolPlatformServices> &services)
{
    s_services = services;
}

void showTransformToolMessage(KoCanvasBase *canvas, const PkString &message,
                              int timeout, TransformToolMessagePriority priority)
{
    TransformToolPlatformServices *canvasServices =
        dynamic_cast<TransformToolPlatformServices *>(canvas);
    const std::shared_ptr<TransformToolPlatformServices> fallback = s_services.lock();
    TransformToolPlatformServices *services = canvasServices ? canvasServices : fallback.get();
    if (services) services->showTransformToolMessage(message, timeout, priority);
}

void showTransformToolNoGslWarning()
{
    const std::shared_ptr<TransformToolPlatformServices> services = s_services.lock();
    if (services) services->showTransformToolNoGslWarning();
}

void useTransformToolCursor(KoCanvasBase *canvas, const TransformCursorDescriptor &cursor)
{
    TransformToolPlatformServices *canvasServices =
        dynamic_cast<TransformToolPlatformServices *>(canvas);
    const std::shared_ptr<TransformToolPlatformServices> fallback = s_services.lock();
    TransformToolPlatformServices *services = canvasServices ? canvasServices : fallback.get();
    if (services) services->useTransformToolCursor(cursor);
}

void resetTransformToolRotationCenterControls(KoCanvasBase *canvas)
{
    TransformToolPlatformServices *canvasServices =
        dynamic_cast<TransformToolPlatformServices *>(canvas);
    const std::shared_ptr<TransformToolPlatformServices> fallback = s_services.lock();
    TransformToolPlatformServices *services = canvasServices ? canvasServices : fallback.get();
    if (services) services->resetTransformToolRotationCenterControls();
}

void setTransformToolImageTooBig(KoCanvasBase *canvas, bool value)
{
    TransformToolPlatformServices *canvasServices =
        dynamic_cast<TransformToolPlatformServices *>(canvas);
    const std::shared_ptr<TransformToolPlatformServices> fallback = s_services.lock();
    TransformToolPlatformServices *services = canvasServices ? canvasServices : fallback.get();
    if (services) services->setTransformToolImageTooBig(value);
}

void updateTransformToolOptions(KoCanvasBase *canvas, bool enabled,
                                const ToolTransformArgs &config)
{
    TransformToolPlatformServices *canvasServices =
        dynamic_cast<TransformToolPlatformServices *>(canvas);
    const std::shared_ptr<TransformToolPlatformServices> fallback = s_services.lock();
    TransformToolPlatformServices *services = canvasServices ? canvasServices : fallback.get();
    if (services) services->updateTransformToolOptions(enabled, config);
}

void setTransformToolApplyResetEnabled(KoCanvasBase *canvas, bool enabled)
{
    TransformToolPlatformServices *canvasServices =
        dynamic_cast<TransformToolPlatformServices *>(canvas);
    const std::shared_ptr<TransformToolPlatformServices> fallback = s_services.lock();
    TransformToolPlatformServices *services = canvasServices ? canvasServices : fallback.get();
    if (services) services->setTransformToolApplyResetEnabled(enabled);
}

TransformToolFactoryDescriptor transformToolFactoryDescriptor()
{
    return {"KisToolTransform",
            "Transform a layer or a selection",
            ToolBoxSection::Transform,
            "krita_tool_transform",
            "Ctrl+T",
            KRITA_TOOL_ACTIVATION_ID,
            2};
}

PkList<TransformToolActionDescriptor> transformToolActionDescriptors()
{
    using A = KisToolTransform::PlatformAction;
    return {
        {"KisToolTransformFree", "Free", A::Free, false, true},
        {"KisToolTransformPerspective", "Perspective", A::Perspective, false, true},
        {"KisToolTransformWarp", "Warp", A::Warp, false, true},
        {"KisToolTransformCage", "Cage", A::Cage, false, true},
        {"KisToolTransformLiquify", "Liquify", A::Liquify, false, true},
        {"KisToolTransformMesh", "Mesh", A::Mesh, false, true},
        {"transform_mirror_horizontal", "Mirror Horizontal", A::MirrorHorizontal},
        {"transform_mirror_vertical", "Mirror Vertical", A::MirrorVertical},
        {"transform_rotate_cw", "Rotate 90 degrees Clockwise", A::RotateClockwise},
        {"transform_rotate_ccw", "Rotate 90 degrees CounterClockwise", A::RotateCounterClockwise},
        {"transform_keep_aspect", "Keep Aspect Ratio", A::KeepAspectRatio, true},
        {"transform_apply", "Apply", A::Apply},
        {"transform_reset", "Reset", A::Reset},
        {"movetool-move-up", "", A::MoveUp},
        {"movetool-move-up-more", "", A::MoveUpMore},
        {"movetool-move-down", "", A::MoveDown},
        {"movetool-move-down-more", "", A::MoveDownMore},
        {"movetool-move-left", "", A::MoveLeft},
        {"movetool-move-left-more", "", A::MoveLeftMore},
        {"movetool-move-right", "", A::MoveRight},
        {"movetool-move-right-more", "", A::MoveRightMore},
        {"increase_brush_size", "", A::IncreaseBrushSize},
        {"decrease_brush_size", "", A::DecreaseBrushSize}
    };
}

PkList<KisToolTransform::PlatformAction> transformToolContextActions(
    KisToolTransform::TransformToolMode mode)
{
    using A = KisToolTransform::PlatformAction;
    PkList<A> result {A::Free, A::Perspective, A::Warp, A::Cage,
                      A::Liquify, A::Mesh};
    if (mode == KisToolTransform::FreeTransformMode) {
        result << A::MirrorHorizontal << A::MirrorVertical
               << A::RotateClockwise << A::RotateCounterClockwise
               << A::KeepAspectRatio;
    }
    result << A::Apply << A::Reset;
    return result;
}

bool dispatchTransformToolAction(KisToolTransform *tool,
                                 KisToolTransform::PlatformAction action,
                                 bool checked)
{
    return tool && tool->dispatchPlatformAction(action, checked);
}

PkList<TransformSubtoolActionDescriptor> transformSubtoolActionDescriptors()
{
    return {
        {"KisToolTransformFree", KisToolTransform::FreeTransformMode},
        {"KisToolTransformPerspective", KisToolTransform::PerspectiveTransformMode},
        {"KisToolTransformWarp", KisToolTransform::WarpTransformMode},
        {"KisToolTransformCage", KisToolTransform::CageTransformMode},
        {"KisToolTransformLiquify", KisToolTransform::LiquifyTransformMode},
        {"KisToolTransformMesh", KisToolTransform::MeshTransformMode}
    };
}

bool dispatchTransformSubtoolAction(KisToolTransformFactory *factory,
                                    KisToolTransform::TransformToolMode mode)
{
    if (!factory) return false;
    factory->activateSubtool(mode);
    return true;
}
