/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#ifndef SVG_TEXT_TOOL_RESOURCES_H
#define SVG_TEXT_TOOL_RESOURCES_H

enum class SvgTextToolPixmap {
    Basic,
    InlineHorizontal,
    InlineVertical,
    OnPath,
    InShape,
    IBeamHorizontal,
    IBeamVertical,
    IBeamHorizontalDone
};

const char *const *svgTextToolCursorPixmap(SvgTextToolPixmap pixmap);
const char *svgTextToolXmlGui();
void svgTextToolResourceAnchor();

void registerSvgTextTool();

#endif
