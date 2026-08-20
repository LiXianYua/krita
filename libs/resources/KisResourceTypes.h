/*
 *  SPDX-FileCopyrightText: 2019 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#ifndef KISRESOURCETYPES_H
#define KISRESOURCETYPES_H

#include <PkString.h>
#include <PkMap.h>
#include "kritaresources_export.h"
/**
 * These namespaces define the type keys and sub-type keys for resource types.
 * The type keys correspond to folders in the resource folder, the sub-type
 * keys to different types that have their own resource loader instance.
 */
namespace ResourceType {
    KRITARESOURCES_EXPORT extern const PkString PaintOpPresets;
    KRITARESOURCES_EXPORT extern const PkString Brushes;
    KRITARESOURCES_EXPORT extern const PkString Gradients;
    KRITARESOURCES_EXPORT extern const PkString Palettes;
    KRITARESOURCES_EXPORT extern const PkString Patterns;
    KRITARESOURCES_EXPORT extern const PkString Workspaces;
    KRITARESOURCES_EXPORT extern const PkString Symbols;
    KRITARESOURCES_EXPORT extern const PkString WindowLayouts;
    KRITARESOURCES_EXPORT extern const PkString Sessions;
    KRITARESOURCES_EXPORT extern const PkString GamutMasks;
    KRITARESOURCES_EXPORT extern const PkString SeExprScripts;
    KRITARESOURCES_EXPORT extern const PkString TaskSets;
    KRITARESOURCES_EXPORT extern const PkString LayerStyles;
    KRITARESOURCES_EXPORT extern const PkString FontFamilies;
    KRITARESOURCES_EXPORT extern const PkString CssStyles;
}

namespace ResourceSubType {
    KRITARESOURCES_EXPORT extern const PkString AbrBrushes;
    KRITARESOURCES_EXPORT extern const PkString GbrBrushes;
    KRITARESOURCES_EXPORT extern const PkString GihBrushes;
    KRITARESOURCES_EXPORT extern const PkString SvgBrushes;
    KRITARESOURCES_EXPORT extern const PkString PngBrushes;
    KRITARESOURCES_EXPORT extern const PkString SegmentedGradients;
    KRITARESOURCES_EXPORT extern const PkString StopGradients;
    KRITARESOURCES_EXPORT extern const PkString KritaPaintOpPresets;
    KRITARESOURCES_EXPORT extern const PkString MyPaintPaintOpPresets;
}

namespace ResourceName {
    // i18n 已移交横切项：KLocalizedString（ki18nc）删除，英文原文保留为 PkString 常量。
    KRITARESOURCES_EXPORT extern const PkString PaintOpPresets;
    KRITARESOURCES_EXPORT extern const PkString Brushes;
    KRITARESOURCES_EXPORT extern const PkString Gradients;
    KRITARESOURCES_EXPORT extern const PkString Palettes;
    KRITARESOURCES_EXPORT extern const PkString Patterns;
    KRITARESOURCES_EXPORT extern const PkString Workspaces;
    KRITARESOURCES_EXPORT extern const PkString Symbols;
    KRITARESOURCES_EXPORT extern const PkString WindowLayouts;
    KRITARESOURCES_EXPORT extern const PkString Sessions;
    KRITARESOURCES_EXPORT extern const PkString GamutMasks;
    KRITARESOURCES_EXPORT extern const PkString SeExprScripts;
    KRITARESOURCES_EXPORT extern const PkString TaskSets;
    KRITARESOURCES_EXPORT extern const PkString LayerStyles;
    KRITARESOURCES_EXPORT extern const PkString FontFamilies;
    KRITARESOURCES_EXPORT extern const PkString CssStyles;

    KRITARESOURCES_EXPORT PkString resourceTypeToName(const PkString &resourceType);

}



#endif // KISRESOURCETYPES_H
