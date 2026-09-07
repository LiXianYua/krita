/*
 * SPDX-FileCopyrightText: 2026 S-09-g
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// D-12 静态注册聚合器（范式 A）：调用全部插件 register 入口。
// 由 kritaplugin_registry 目标链入全部插件静态库后编译链接。
#include "register_all_plugins.h"

#include <mutex>

// ---- forward declarations ----
extern "C" bool registerEXRExportFilter();
extern "C" bool registerexrImportFilter();
extern "C" bool registerHeifExportFilter();
extern "C" bool registerHeifImportFilter();
extern "C" bool registerjp2ImportFilter();
extern "C" bool registerJPEGXLExportFilter();
extern "C" bool registerJPEGXLImportFilter();
extern "C" bool registerKisBrushExportFilter();
extern "C" bool registerKisBrushImportFilter();
extern "C" bool registerKisCSVExportFilter();
extern "C" bool registerKisCSVImportFilter();
extern "C" bool registerKisGIFExportFilter();
extern "C" bool registerKisGIFImportFilter();
extern "C" bool registerKisHeightMapExportFilter();
extern "C" bool registerKisHeightMapImportFilter();
extern "C" bool registerKisJPEGExportFilter();
extern "C" bool registerKisJPEGImportFilter();
extern "C" bool registerKisPDFImportFilter();
extern "C" bool registerKisPNGExportFilter();
extern "C" bool registerKisPNGImportFilter();
extern "C" bool registerKisQImageIOExportFilter();
extern "C" bool registerKisQImageIOImportFilter();
extern "C" bool registerKisRawImportFilter();
extern "C" bool registerKisSpriterExportFilter();
extern "C" bool registerKisSVGImportFilter();
extern "C" bool registerKisTGAExportFilter();
extern "C" bool registerKisTGAImportFilter();
extern "C" bool registerKisTIFFExportFilter();
extern "C" bool registerKisTIFFImportFilter();
extern "C" bool registerKisWebPExportFilter();
extern "C" bool registerKisWebPImportFilter();
extern "C" bool registerKisXCFImportFilter();
extern "C" bool registerKraExportFilter();
extern "C" bool registerKraImportFilter();
extern "C" bool registerKrzExportFilter();
extern "C" bool registerOraExportFilter();
extern "C" bool registerOraImportFilter();
extern "C" bool registerpsdExportFilter();
extern "C" bool registerpsdImportFilter();
extern "C" bool registerQMLExportFilter();
extern "C" bool registerRGBEExportFilter();
extern "C" bool registerRGBEImportFilter();
void registerAssistantFactories();
void registerColorSpaceExtensions();
void registerDefaultToolPlugin();
void registerDefaultTools();
void registerKarbonTools();
void registerLcmsEngine();
void registerPathShapes();
void registerSelectionTools();
void registerSvgTextTool();
void registerToolCrop();
void registerToolDyna();
void registerToolEncloseAndFill();
void registerToolKnife();
void registerToolLazyBrush();
void registerToolPolygon();
void registerToolPolyline();
void registerToolSmartPatch();
void registerToolTransformPlugin();

void registerAllPlugins()
{
    static std::once_flag once;
    std::call_once(once, [] {
        (void)registerEXRExportFilter();
        (void)registerexrImportFilter();
        (void)registerHeifExportFilter();
        (void)registerHeifImportFilter();
        (void)registerjp2ImportFilter();
        (void)registerJPEGXLExportFilter();
        (void)registerJPEGXLImportFilter();
        (void)registerKisBrushExportFilter();
        (void)registerKisBrushImportFilter();
        (void)registerKisCSVExportFilter();
        (void)registerKisCSVImportFilter();
        (void)registerKisGIFExportFilter();
        (void)registerKisGIFImportFilter();
        (void)registerKisHeightMapExportFilter();
        (void)registerKisHeightMapImportFilter();
        (void)registerKisJPEGExportFilter();
        (void)registerKisJPEGImportFilter();
        (void)registerKisPDFImportFilter();
        (void)registerKisPNGExportFilter();
        (void)registerKisPNGImportFilter();
        (void)registerKisQImageIOExportFilter();
        (void)registerKisQImageIOImportFilter();
        (void)registerKisRawImportFilter();
        (void)registerKisSpriterExportFilter();
        (void)registerKisSVGImportFilter();
        (void)registerKisTGAExportFilter();
        (void)registerKisTGAImportFilter();
        (void)registerKisTIFFExportFilter();
        (void)registerKisTIFFImportFilter();
        (void)registerKisWebPExportFilter();
        (void)registerKisWebPImportFilter();
        (void)registerKisXCFImportFilter();
        (void)registerKraExportFilter();
        (void)registerKraImportFilter();
        (void)registerKrzExportFilter();
        (void)registerOraExportFilter();
        (void)registerOraImportFilter();
        (void)registerpsdExportFilter();
        (void)registerpsdImportFilter();
        (void)registerQMLExportFilter();
        (void)registerRGBEExportFilter();
        (void)registerRGBEImportFilter();
        registerAssistantFactories();
        registerColorSpaceExtensions();
        registerDefaultToolPlugin();
        registerDefaultTools();
        registerKarbonTools();
        registerLcmsEngine();
        registerPathShapes();
        registerSelectionTools();
        registerSvgTextTool();
        registerToolCrop();
        registerToolDyna();
        registerToolEncloseAndFill();
        registerToolKnife();
        registerToolLazyBrush();
        registerToolPolygon();
        registerToolPolyline();
        registerToolSmartPatch();
        registerToolTransformPlugin();
    });
}
