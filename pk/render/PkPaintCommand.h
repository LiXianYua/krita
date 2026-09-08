#pragma once
#include <variant>
#include "PkPen.h"
#include "PkBrush.h"
#include "PkTransform.h"
#include "PkPainterPath.h"
#include "PkPolygon.h"
#include "PkLine.h"
#include "PkRect.h"
#include "PkImage.h"
#include "../font/PkFont.h"
#include "../string/PkString.h"
struct PkSaveCommand {};
struct PkRestoreCommand {};
struct PkSetPenCommand { PkPen pen; };
struct PkSetBrushCommand { PkBrush brush; };
struct PkSetTransformCommand { PkTransform transform; bool combine; };
struct PkSetRenderHintCommand { unsigned hint; bool enabled; };
struct PkSetClipRectCommand { PkRectF rect; Pk::ClipOperation operation; };
struct PkDrawLineCommand { PkLineF line; };
struct PkDrawRectCommand { PkRectF rect; };
struct PkDrawEllipseCommand { PkRectF rect; };
struct PkDrawArcCommand { PkRectF rect; int startAngle16; int spanAngle16; };
struct PkDrawPathCommand { PkPainterPath path; };
struct PkDrawPolygonCommand { PkPolygonF polygon; };
struct PkDrawImageCommand { PkRectF target; PkImage image; };

// ── S-09-g 扩锁：QPainter→PkPainter 实际调用面新增的命令 ──────────────────────
// 每条对应一个实测调用点类别（计数见 libs/canvas+plugins 扫描）：
//   setCompositionMode 9 / fillRect 9 / setOpacity 9 / setClipPath 12 /
//   fillPath 8 / drawPoint 4 / strokePath 3 / drawPixmap 4 / drawTiledPixmap 1 /
//   drawText 2 / setFont 1
struct PkSetCompositionModeCommand { Pk::CompositionMode mode; };
struct PkSetOpacityCommand { qreal opacity; };
struct PkSetClipPathCommand { PkPainterPath path; Pk::ClipOperation operation; };
struct PkFillRectCommand { PkRectF rect; PkBrush brush; };
struct PkFillPathCommand { PkPainterPath path; PkBrush brush; };
// Image-backed fill used by the baked vector-pattern renderer. Own the image
// snapshot in the render command, without adding image ownership to geometry.
struct PkFillTexturePathCommand { PkPainterPath path; PkImage image; PkTransform transform; };
struct PkDrawPointCommand { PkPointF point; };
struct PkStrokePathCommand { PkPainterPath path; PkPen pen; };
struct PkDrawPixmapCommand { PkRectF target; PkImage image; PkRectF source; };
struct PkDrawTiledPixmapCommand { PkRectF rect; PkImage image; PkPointF offset; };
struct PkSetFontCommand { PkFont font; };
struct PkDrawTextAtPointCommand { PkPointF position; PkString text; };
struct PkDrawTextInRectCommand { PkRectF rect; PkString text; };

using PkPaintCommand = std::variant<PkSaveCommand,PkRestoreCommand,PkSetPenCommand,PkSetBrushCommand,PkSetTransformCommand,PkSetRenderHintCommand,PkSetClipRectCommand,PkDrawLineCommand,PkDrawRectCommand,PkDrawEllipseCommand,PkDrawArcCommand,PkDrawPathCommand,PkDrawPolygonCommand,PkDrawImageCommand,PkSetCompositionModeCommand,PkSetOpacityCommand,PkSetClipPathCommand,PkFillRectCommand,PkFillPathCommand,PkFillTexturePathCommand,PkDrawPointCommand,PkStrokePathCommand,PkDrawPixmapCommand,PkDrawTiledPixmapCommand,PkSetFontCommand,PkDrawTextAtPointCommand,PkDrawTextInRectCommand>;
class PkPainterBackend {
public:
    virtual ~PkPainterBackend() = default;
    virtual void submit(const PkPaintCommand&) = 0;
    virtual qreal devicePixelRatio() const { return 1.0; }
};
