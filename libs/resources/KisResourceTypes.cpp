/*
 * SPDX-FileCopyrightText: 2021 Halla Rempt <halla@valdyas.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "KisResourceTypes.h"
#include <PkGlobal.h>

namespace ResourceType {
    const PkString PaintOpPresets {"paintoppresets"};
    const PkString Brushes {"brushes"};
    const PkString Gradients {"gradients"};
    const PkString Palettes {"palettes"};
    const PkString Patterns {"patterns"};
    const PkString Workspaces {"workspaces"};
    const PkString Symbols {"symbols"};
    const PkString WindowLayouts {"windowlayouts"};
    const PkString Sessions {"sessions"};
    const PkString GamutMasks {"gamutmasks"};
    const PkString SeExprScripts {"seexpr_scripts"};
    const PkString TaskSets {"tasksets"};
    const PkString LayerStyles {"layerstyles"};
    const PkString FontFamilies {"fontfamilies"};
    const PkString CssStyles {"css_styles"};
}

namespace ResourceSubType {
    const PkString AbrBrushes {"abr_brushes"};
    const PkString GbrBrushes {"gbr_brushes"};
    const PkString GihBrushes {"gih_brushes"};
    const PkString SvgBrushes {"svg_brushes"};
    const PkString PngBrushes {"png_brushes"};
    const PkString SegmentedGradients {"segmented_gradients"};
    const PkString StopGradients {"stop_gradients"};
    const PkString KritaPaintOpPresets {"krita_paintop_presets"};
    const PkString MyPaintPaintOpPresets {"mypaint_paintop_presets"};
}

namespace ResourceName {
    // i18n 已移交横切项：ki18nc 的上下文参数删除，英文原文保留。
    const PkString PaintOpPresets {"Brush Presets"};
    const PkString Brushes {"Brush Tips"};
    const PkString Gradients {"Gradients"};
    const PkString Palettes {"Palettes"};
    const PkString Patterns {"Patterns"};
    const PkString Workspaces {"Workspaces"};
    const PkString Symbols {"Symbol Libraries"};
    const PkString WindowLayouts {"Window Layouts"};
    const PkString Sessions {"Sessions"};
    const PkString GamutMasks {"Gamut Masks"};
    const PkString SeExprScripts {"SeExpr Scripts"};
    const PkString TaskSets {"Task Sets"};
    const PkString LayerStyles {"Layer Styles"};
    const PkString FontFamilies {"Font Families"};
    const PkString CssStyles {"Style Presets"};
}

PkString ResourceName::resourceTypeToName(const PkString &resourceType)
{
    static const PkMap<PkString, PkString> typeMap = []() {
        // 全局单例检查删除（无 Qt 单例）。
        PkMap<PkString, PkString> typeMap;
        typeMap[ResourceType::PaintOpPresets] = ResourceName::PaintOpPresets;
        typeMap[ResourceType::Brushes] = ResourceName::Brushes;
        typeMap[ResourceType::Gradients] = ResourceName::Gradients;
        typeMap[ResourceType::Palettes] = ResourceName::Palettes;
        typeMap[ResourceType::Patterns] = ResourceName::Patterns;
        typeMap[ResourceType::Workspaces] = ResourceName::Workspaces;
        typeMap[ResourceType::Symbols] = ResourceName::Symbols;
        typeMap[ResourceType::WindowLayouts] = ResourceName::WindowLayouts;
        typeMap[ResourceType::Sessions] = ResourceName::Sessions;
        typeMap[ResourceType::GamutMasks] = ResourceName::GamutMasks;
        typeMap[ResourceType::SeExprScripts] = ResourceName::SeExprScripts;
        typeMap[ResourceType::TaskSets] = ResourceName::TaskSets;
        typeMap[ResourceType::LayerStyles] = ResourceName::LayerStyles;
        typeMap[ResourceType::FontFamilies] = ResourceName::FontFamilies;
        typeMap[ResourceType::CssStyles] = ResourceName::CssStyles;
        return typeMap;
    }();

    Q_ASSERT(typeMap.contains(resourceType));

    return typeMap[resourceType];

}
