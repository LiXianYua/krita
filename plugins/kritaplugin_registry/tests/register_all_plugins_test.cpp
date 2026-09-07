/*
 * SPDX-FileCopyrightText: 2026 S-09-g
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "register_all_plugins.h"

#include <cstdlib>
#include <iostream>
#include <map>
#include <string>

namespace
{
std::map<std::string, int> calls;

void record(const char *name)
{
    ++calls[name];
}

void requireCalledExactlyOnce(const char *name)
{
    if (calls[name] != 1) {
        std::cerr << name << " called " << calls[name] << " times\n";
        std::exit(EXIT_FAILURE);
    }
}
}

#define DEFINE_IMPEX_REGISTRATION(name) \
    extern "C" bool name()              \
    {                                    \
        record(#name);                   \
        return true;                     \
    }

DEFINE_IMPEX_REGISTRATION(registerEXRExportFilter)
DEFINE_IMPEX_REGISTRATION(registerexrImportFilter)
DEFINE_IMPEX_REGISTRATION(registerHeifExportFilter)
DEFINE_IMPEX_REGISTRATION(registerHeifImportFilter)
DEFINE_IMPEX_REGISTRATION(registerjp2ImportFilter)
DEFINE_IMPEX_REGISTRATION(registerJPEGXLExportFilter)
DEFINE_IMPEX_REGISTRATION(registerJPEGXLImportFilter)
DEFINE_IMPEX_REGISTRATION(registerKisBrushExportFilter)
DEFINE_IMPEX_REGISTRATION(registerKisBrushImportFilter)
DEFINE_IMPEX_REGISTRATION(registerKisCSVExportFilter)
DEFINE_IMPEX_REGISTRATION(registerKisCSVImportFilter)
DEFINE_IMPEX_REGISTRATION(registerKisGIFExportFilter)
DEFINE_IMPEX_REGISTRATION(registerKisGIFImportFilter)
DEFINE_IMPEX_REGISTRATION(registerKisHeightMapExportFilter)
DEFINE_IMPEX_REGISTRATION(registerKisHeightMapImportFilter)
DEFINE_IMPEX_REGISTRATION(registerKisJPEGExportFilter)
DEFINE_IMPEX_REGISTRATION(registerKisJPEGImportFilter)
DEFINE_IMPEX_REGISTRATION(registerKisPDFImportFilter)
DEFINE_IMPEX_REGISTRATION(registerKisPNGExportFilter)
DEFINE_IMPEX_REGISTRATION(registerKisPNGImportFilter)
DEFINE_IMPEX_REGISTRATION(registerKisQImageIOExportFilter)
DEFINE_IMPEX_REGISTRATION(registerKisQImageIOImportFilter)
DEFINE_IMPEX_REGISTRATION(registerKisRawImportFilter)
DEFINE_IMPEX_REGISTRATION(registerKisSpriterExportFilter)
DEFINE_IMPEX_REGISTRATION(registerKisSVGImportFilter)
DEFINE_IMPEX_REGISTRATION(registerKisTGAExportFilter)
DEFINE_IMPEX_REGISTRATION(registerKisTGAImportFilter)
DEFINE_IMPEX_REGISTRATION(registerKisTIFFExportFilter)
DEFINE_IMPEX_REGISTRATION(registerKisTIFFImportFilter)
DEFINE_IMPEX_REGISTRATION(registerKisWebPExportFilter)
DEFINE_IMPEX_REGISTRATION(registerKisWebPImportFilter)
DEFINE_IMPEX_REGISTRATION(registerKisXCFImportFilter)
DEFINE_IMPEX_REGISTRATION(registerKraExportFilter)
DEFINE_IMPEX_REGISTRATION(registerKraImportFilter)
DEFINE_IMPEX_REGISTRATION(registerKrzExportFilter)
DEFINE_IMPEX_REGISTRATION(registerOraExportFilter)
DEFINE_IMPEX_REGISTRATION(registerOraImportFilter)
DEFINE_IMPEX_REGISTRATION(registerpsdExportFilter)
DEFINE_IMPEX_REGISTRATION(registerpsdImportFilter)
DEFINE_IMPEX_REGISTRATION(registerQMLExportFilter)
DEFINE_IMPEX_REGISTRATION(registerRGBEExportFilter)
DEFINE_IMPEX_REGISTRATION(registerRGBEImportFilter)

#define DEFINE_PLUGIN_REGISTRATION(name) \
    void name()                          \
    {                                    \
        record(#name);                   \
    }

DEFINE_PLUGIN_REGISTRATION(registerAssistantFactories)
DEFINE_PLUGIN_REGISTRATION(registerColorSpaceExtensions)
DEFINE_PLUGIN_REGISTRATION(registerDefaultToolPlugin)
DEFINE_PLUGIN_REGISTRATION(registerDefaultTools)
DEFINE_PLUGIN_REGISTRATION(registerKarbonTools)
DEFINE_PLUGIN_REGISTRATION(registerLcmsEngine)
DEFINE_PLUGIN_REGISTRATION(registerPathShapes)
DEFINE_PLUGIN_REGISTRATION(registerSelectionTools)
DEFINE_PLUGIN_REGISTRATION(registerSvgTextTool)
DEFINE_PLUGIN_REGISTRATION(registerToolCrop)
DEFINE_PLUGIN_REGISTRATION(registerToolDyna)
DEFINE_PLUGIN_REGISTRATION(registerToolEncloseAndFill)
DEFINE_PLUGIN_REGISTRATION(registerToolKnife)
DEFINE_PLUGIN_REGISTRATION(registerToolLazyBrush)
DEFINE_PLUGIN_REGISTRATION(registerToolPolygon)
DEFINE_PLUGIN_REGISTRATION(registerToolPolyline)
DEFINE_PLUGIN_REGISTRATION(registerToolSmartPatch)
DEFINE_PLUGIN_REGISTRATION(registerToolTransformPlugin)

int main()
{
    registerAllPlugins();
    registerAllPlugins();

#define REQUIRE_ONCE(name) requireCalledExactlyOnce(#name)
    REQUIRE_ONCE(registerEXRExportFilter);
    REQUIRE_ONCE(registerexrImportFilter);
    REQUIRE_ONCE(registerHeifExportFilter);
    REQUIRE_ONCE(registerHeifImportFilter);
    REQUIRE_ONCE(registerjp2ImportFilter);
    REQUIRE_ONCE(registerJPEGXLExportFilter);
    REQUIRE_ONCE(registerJPEGXLImportFilter);
    REQUIRE_ONCE(registerKisBrushExportFilter);
    REQUIRE_ONCE(registerKisBrushImportFilter);
    REQUIRE_ONCE(registerKisCSVExportFilter);
    REQUIRE_ONCE(registerKisCSVImportFilter);
    REQUIRE_ONCE(registerKisGIFExportFilter);
    REQUIRE_ONCE(registerKisGIFImportFilter);
    REQUIRE_ONCE(registerKisHeightMapExportFilter);
    REQUIRE_ONCE(registerKisHeightMapImportFilter);
    REQUIRE_ONCE(registerKisJPEGExportFilter);
    REQUIRE_ONCE(registerKisJPEGImportFilter);
    REQUIRE_ONCE(registerKisPDFImportFilter);
    REQUIRE_ONCE(registerKisPNGExportFilter);
    REQUIRE_ONCE(registerKisPNGImportFilter);
    REQUIRE_ONCE(registerKisQImageIOExportFilter);
    REQUIRE_ONCE(registerKisQImageIOImportFilter);
    REQUIRE_ONCE(registerKisRawImportFilter);
    REQUIRE_ONCE(registerKisSpriterExportFilter);
    REQUIRE_ONCE(registerKisSVGImportFilter);
    REQUIRE_ONCE(registerKisTGAExportFilter);
    REQUIRE_ONCE(registerKisTGAImportFilter);
    REQUIRE_ONCE(registerKisTIFFExportFilter);
    REQUIRE_ONCE(registerKisTIFFImportFilter);
    REQUIRE_ONCE(registerKisWebPExportFilter);
    REQUIRE_ONCE(registerKisWebPImportFilter);
    REQUIRE_ONCE(registerKisXCFImportFilter);
    REQUIRE_ONCE(registerKraExportFilter);
    REQUIRE_ONCE(registerKraImportFilter);
    REQUIRE_ONCE(registerKrzExportFilter);
    REQUIRE_ONCE(registerOraExportFilter);
    REQUIRE_ONCE(registerOraImportFilter);
    REQUIRE_ONCE(registerpsdExportFilter);
    REQUIRE_ONCE(registerpsdImportFilter);
    REQUIRE_ONCE(registerQMLExportFilter);
    REQUIRE_ONCE(registerRGBEExportFilter);
    REQUIRE_ONCE(registerRGBEImportFilter);
    REQUIRE_ONCE(registerAssistantFactories);
    REQUIRE_ONCE(registerColorSpaceExtensions);
    REQUIRE_ONCE(registerDefaultToolPlugin);
    REQUIRE_ONCE(registerDefaultTools);
    REQUIRE_ONCE(registerKarbonTools);
    REQUIRE_ONCE(registerLcmsEngine);
    REQUIRE_ONCE(registerPathShapes);
    REQUIRE_ONCE(registerSelectionTools);
    REQUIRE_ONCE(registerSvgTextTool);
    REQUIRE_ONCE(registerToolCrop);
    REQUIRE_ONCE(registerToolDyna);
    REQUIRE_ONCE(registerToolEncloseAndFill);
    REQUIRE_ONCE(registerToolKnife);
    REQUIRE_ONCE(registerToolLazyBrush);
    REQUIRE_ONCE(registerToolPolygon);
    REQUIRE_ONCE(registerToolPolyline);
    REQUIRE_ONCE(registerToolSmartPatch);
    REQUIRE_ONCE(registerToolTransformPlugin);
    return EXIT_SUCCESS;
}
