#pragma once

#include <PkList.h>
#include <PkString.h>
#include <PkGlobal.h>
#include <PkPainter.h>

#include <memory>

#include "kis_tool_transform.h"

class KoCanvasBase;

class KRITATOOLTRANSFORM_EXPORT TransformToolPainter {
public:
    explicit TransformToolPainter(PkPainter &painter);

    void save();
    void restore();
    void setOpacity(qreal opacity);
    PkTransform transform() const;
    void setTransform(const PkTransform &transform, bool combine = false);
    void setPen(const PkPen &pen);
    void setBrush(const PkBrush &brush);
    PkBrush brush() const;
    void drawImage(const PkPointF &position, const PkImage &image);
    void drawPath(const PkPainterPath &path);
    void drawLine(const PkPointF &from, const PkPointF &to);
    void drawEllipse(const PkRectF &rect);

    operator PkPainter &() { return m_painter; }

private:
    template<typename Draw>
    void drawWithOpacity(Draw draw);
    static PkColor withOpacity(const PkColor &color, qreal opacity);
    static PkImage withOpacity(const PkImage &image, qreal opacity);

    PkPainter &m_painter;
    PkList<qreal> m_opacityStack {1.0};
};

enum class TransformHandleStyle {
    PrimarySelection,
    HighlightedPrimaryHandles,
    HighlightedPrimaryHandlesWithSolidOutline,
    SelectedPrimaryHandles
};

class KRITATOOLTRANSFORM_EXPORT TransformToolHandlePainter {
public:
    TransformToolHandlePainter(TransformToolPainter &painter,
                               qreal handleRadius,
                               int decorationThickness);
    ~TransformToolHandlePainter();

    TransformToolHandlePainter(const TransformToolHandlePainter &) = delete;
    TransformToolHandlePainter &operator=(const TransformToolHandlePainter &) = delete;

    void setHandleStyle(TransformHandleStyle style);
    void drawHandleCircle(const PkPointF &center);
    void drawHandleSmallCircle(const PkPointF &center);
    void drawConnectionLine(const PkPointF &from, const PkPointF &to);
    void drawPath(const PkPainterPath &path);

private:
    PkPen handlePen() const;
    PkBrush handleBrush() const;
    void drawLinePasses(const PkPointF &from, const PkPointF &to);
    void drawPathPasses(const PkPainterPath &path);

    TransformToolPainter &m_painter;
    PkTransform m_painterTransform;
    qreal m_handleRadius {0.0};
    int m_decorationThickness {1};
    TransformHandleStyle m_style {TransformHandleStyle::PrimarySelection};
};

enum class TransformToolMessagePriority { High, Medium, Low };
enum class TransformCursorKind {
    Arrow, PointingHand, Cross, SizeAll, SizeHorizontal, SizeVertical,
    SizeForwardDiagonal, SizeBackwardDiagonal, SplitHorizontal, SplitVertical,
    Blank, Wait, RotateHandles, Shear
};
struct TransformCursorDescriptor {
    TransformCursorKind kind {TransformCursorKind::Arrow};
    qreal rotationRadians {0.0};
};

class TransformToolPlatformServices {
public:
    virtual ~TransformToolPlatformServices() = default;
    virtual void showTransformToolMessage(const PkString &message, int timeout,
                                          TransformToolMessagePriority priority) = 0;
    virtual void showTransformToolNoGslWarning() = 0;
    virtual void useTransformToolCursor(const TransformCursorDescriptor &cursor) = 0;
    virtual void resetTransformToolRotationCenterControls() = 0;
    virtual void setTransformToolImageTooBig(bool value) = 0;
    virtual void updateTransformToolOptions(bool enabled,
                                            const ToolTransformArgs &config) = 0;
    virtual void setTransformToolApplyResetEnabled(bool enabled) = 0;
};

KRITATOOLTRANSFORM_EXPORT void showTransformToolMessage(
    KoCanvasBase *canvas, const PkString &message,
    int timeout, TransformToolMessagePriority priority);
KRITATOOLTRANSFORM_EXPORT void showTransformToolNoGslWarning();
KRITATOOLTRANSFORM_EXPORT void setTransformToolPlatformServices(
    const std::shared_ptr<TransformToolPlatformServices> &services);
KRITATOOLTRANSFORM_EXPORT void useTransformToolCursor(
    KoCanvasBase *canvas, const TransformCursorDescriptor &cursor);
KRITATOOLTRANSFORM_EXPORT void resetTransformToolRotationCenterControls(
    KoCanvasBase *canvas);
KRITATOOLTRANSFORM_EXPORT void setTransformToolImageTooBig(
    KoCanvasBase *canvas, bool value);
KRITATOOLTRANSFORM_EXPORT void updateTransformToolOptions(
    KoCanvasBase *canvas, bool enabled, const ToolTransformArgs &config);
KRITATOOLTRANSFORM_EXPORT void setTransformToolApplyResetEnabled(
    KoCanvasBase *canvas, bool enabled);

struct TransformToolActionDescriptor {
    PkString actionId;
    PkString label;
    KisToolTransform::PlatformAction action;
    bool checkable {false};
    bool alwaysEnabled {false};
};

KRITATOOLTRANSFORM_EXPORT PkList<TransformToolActionDescriptor>
transformToolActionDescriptors();
KRITATOOLTRANSFORM_EXPORT PkList<KisToolTransform::PlatformAction> transformToolContextActions(
    KisToolTransform::TransformToolMode mode);
KRITATOOLTRANSFORM_EXPORT bool dispatchTransformToolAction(
    KisToolTransform *tool, KisToolTransform::PlatformAction action,
    bool checked = false);

struct TransformSubtoolActionDescriptor {
    PkString actionId;
    KisToolTransform::TransformToolMode mode;
};

KRITATOOLTRANSFORM_EXPORT PkList<TransformSubtoolActionDescriptor>
transformSubtoolActionDescriptors();
KRITATOOLTRANSFORM_EXPORT bool dispatchTransformSubtoolAction(
    KisToolTransformFactory *factory, KisToolTransform::TransformToolMode mode);
