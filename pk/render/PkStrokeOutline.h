#pragma once

#include <PkPainterPath.h>
#include <PkPen.h>
#include <PkRect.h>

namespace PkRender {
// Native stroked outline shared by tool decorations and the image rasterizer.
// An empty clip disables the dash optimization clip.
PkPainterPath createStrokeOutline(const PkPainterPath &path, const PkPen &pen,
                                  const PkRect &clip = {});
}
