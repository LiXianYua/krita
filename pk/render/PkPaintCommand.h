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
struct PkSaveCommand {};
struct PkRestoreCommand {};
struct PkSetPenCommand { PkPen pen; };
struct PkSetBrushCommand { PkBrush brush; };
struct PkSetTransformCommand { PkTransform transform; bool combine; };
struct PkSetRenderHintCommand { unsigned hint; bool enabled; };
struct PkSetClipRectCommand { PkRectF rect; Qt::ClipOperation operation; };
struct PkDrawLineCommand { PkLineF line; };
struct PkDrawRectCommand { PkRectF rect; };
struct PkDrawEllipseCommand { PkRectF rect; };
struct PkDrawArcCommand { PkRectF rect; int startAngle16; int spanAngle16; };
struct PkDrawPathCommand { PkPainterPath path; };
struct PkDrawPolygonCommand { PkPolygonF polygon; };
struct PkDrawImageCommand { PkRectF target; PkImage image; };
using PkPaintCommand = std::variant<PkSaveCommand,PkRestoreCommand,PkSetPenCommand,PkSetBrushCommand,PkSetTransformCommand,PkSetRenderHintCommand,PkSetClipRectCommand,PkDrawLineCommand,PkDrawRectCommand,PkDrawEllipseCommand,PkDrawArcCommand,PkDrawPathCommand,PkDrawPolygonCommand,PkDrawImageCommand>;
class PkPainterBackend { public: virtual ~PkPainterBackend() = default; virtual void submit(const PkPaintCommand&) = 0; };
