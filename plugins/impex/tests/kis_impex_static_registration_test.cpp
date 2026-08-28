/*
 * SPDX-FileCopyrightText: 2026 S-09-c
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <cstdlib>
#include <iostream>
#include <string>

#ifndef TASK6_RED_ONLY
#include <KisImportExportManager.h>

#include <iterator>
#include <set>
#include <typeinfo>
#endif

#if defined(__GNUC__)
#define KIS_WEAK_REGISTRATION __attribute__((weak))
#else
#define KIS_WEAK_REGISTRATION
#endif

extern "C" bool registerKisBrushExportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerKisBrushImportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerKisCSVExportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerKisCSVImportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerEXRExportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerexrImportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerKisGIFExportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerKisGIFImportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerHeifExportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerHeifImportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerKisHeightMapExportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerKisHeightMapImportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerjp2ImportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerKisJPEGExportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerKisJPEGImportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerJPEGXLExportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerJPEGXLImportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerKraExportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerKraImportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerKrzExportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerOraExportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerOraImportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerKisPDFImportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerKisPNGExportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerKisPNGImportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerpsdExportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerpsdImportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerKisQImageIOExportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerKisQImageIOImportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerQMLExportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerKisRawImportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerRGBEExportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerRGBEImportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerKisSpriterExportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerKisSVGImportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerKisTGAExportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerKisTGAImportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerKisTIFFExportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerKisTIFFImportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerKisWebPExportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerKisWebPImportFilter() KIS_WEAK_REGISTRATION;
extern "C" bool registerKisXCFImportFilter() KIS_WEAK_REGISTRATION;

namespace
{

using RegistrationFunction = bool (*)();

struct NamedRegistration
{
    RegistrationFunction function;
    const char *name;
};

const NamedRegistration registrations[] = {
    {&registerKisBrushExportFilter, "registerKisBrushExportFilter"},
    {&registerKisBrushImportFilter, "registerKisBrushImportFilter"},
    {&registerKisCSVExportFilter, "registerKisCSVExportFilter"},
    {&registerKisCSVImportFilter, "registerKisCSVImportFilter"},
    {&registerEXRExportFilter, "registerEXRExportFilter"},
    {&registerexrImportFilter, "registerexrImportFilter"},
    {&registerKisGIFExportFilter, "registerKisGIFExportFilter"},
    {&registerKisGIFImportFilter, "registerKisGIFImportFilter"},
    {&registerHeifExportFilter, "registerHeifExportFilter"},
    {&registerHeifImportFilter, "registerHeifImportFilter"},
    {&registerKisHeightMapExportFilter, "registerKisHeightMapExportFilter"},
    {&registerKisHeightMapImportFilter, "registerKisHeightMapImportFilter"},
    {&registerjp2ImportFilter, "registerjp2ImportFilter"},
    {&registerKisJPEGExportFilter, "registerKisJPEGExportFilter"},
    {&registerKisJPEGImportFilter, "registerKisJPEGImportFilter"},
    {&registerJPEGXLExportFilter, "registerJPEGXLExportFilter"},
    {&registerJPEGXLImportFilter, "registerJPEGXLImportFilter"},
    {&registerKraExportFilter, "registerKraExportFilter"},
    {&registerKraImportFilter, "registerKraImportFilter"},
    {&registerKrzExportFilter, "registerKrzExportFilter"},
    {&registerOraExportFilter, "registerOraExportFilter"},
    {&registerOraImportFilter, "registerOraImportFilter"},
    {&registerKisPDFImportFilter, "registerKisPDFImportFilter"},
    {&registerKisPNGExportFilter, "registerKisPNGExportFilter"},
    {&registerKisPNGImportFilter, "registerKisPNGImportFilter"},
    {&registerpsdExportFilter, "registerpsdExportFilter"},
    {&registerpsdImportFilter, "registerpsdImportFilter"},
    {&registerKisQImageIOExportFilter, "registerKisQImageIOExportFilter"},
    {&registerKisQImageIOImportFilter, "registerKisQImageIOImportFilter"},
    {&registerQMLExportFilter, "registerQMLExportFilter"},
    {&registerKisRawImportFilter, "registerKisRawImportFilter"},
    {&registerRGBEExportFilter, "registerRGBEExportFilter"},
    {&registerRGBEImportFilter, "registerRGBEImportFilter"},
    {&registerKisSpriterExportFilter, "registerKisSpriterExportFilter"},
    {&registerKisSVGImportFilter, "registerKisSVGImportFilter"},
    {&registerKisTGAExportFilter, "registerKisTGAExportFilter"},
    {&registerKisTGAImportFilter, "registerKisTGAImportFilter"},
    {&registerKisTIFFExportFilter, "registerKisTIFFExportFilter"},
    {&registerKisTIFFImportFilter, "registerKisTIFFImportFilter"},
    {&registerKisWebPExportFilter, "registerKisWebPExportFilter"},
    {&registerKisWebPImportFilter, "registerKisWebPImportFilter"},
    {&registerKisXCFImportFilter, "registerKisXCFImportFilter"},
};

[[noreturn]] void fail(const std::string &message)
{
    std::cerr << message << '\n';
    std::exit(1);
}

#ifndef TASK6_RED_ONLY
const char *const expectedImportMimeTypes[] = {
    "application/pdf", "application/x-extension-exr", "application/x-krita",
    "application/x-krita-archive", "application/x-krita-paintoppreset", "image/avif",
    "image/bmp", "image/gif", "image/heic", "image/jp2", "image/jpeg",
    "image/jpeg2000", "image/jpeg2000-image", "image/jpx", "image/jxl",
    "image/openraster", "image/photoshop", "image/png", "image/svg+xml", "image/tiff",
    "image/vnd.adobe.photoshop", "image/vnd.microsoft.icon", "image/vnd.radiance",
    "image/webp", "image/x-exr", "image/x-gimp-brush", "image/x-gimp-brush-animated",
    "image/x-jpeg2000-image", "image/x-krita-raw", "image/x-photoshop",
    "image/x-portable-bitmap", "image/x-portable-graymap", "image/x-portable-pixmap",
    "image/x-psb", "image/x-psd", "image/x-r16", "image/x-r32", "image/x-r8",
    "image/x-tga", "image/x-xbitmap", "image/x-xcf", "image/x-xpixmap", "text/csv",
};

const char *const expectedExportMimeTypes[] = {
    "application/x-extension-exr", "application/x-krita", "application/x-krita-archive",
    "application/x-krita-paintoppreset", "application/x-spriter", "image/avif", "image/bmp",
    "image/gif", "image/heic", "image/jpeg", "image/jxl", "image/openraster",
    "image/photoshop", "image/png", "image/tiff", "image/vnd.adobe.photoshop",
    "image/vnd.microsoft.icon", "image/vnd.radiance", "image/webp", "image/x-exr",
    "image/x-gimp-brush", "image/x-gimp-brush-animated", "image/x-photoshop",
    "image/x-portable-bitmap", "image/x-portable-graymap", "image/x-portable-pixmap",
    "image/x-psd", "image/x-r16", "image/x-r32", "image/x-r8", "image/x-tga",
    "image/x-xbitmap", "image/x-xpixmap", "text/csv", "text/x-qml",
};

std::set<std::string> toSet(const PkStringList &values)
{
    std::set<std::string> result;
    for (const PkString &value : values) {
        result.insert(value.PkToUtf8());
    }
    return result;
}

template<std::size_t Size>
std::set<std::string> expectedSet(const char *const (&values)[Size])
{
    return std::set<std::string>(std::begin(values), std::end(values));
}

void requireFactory(const char *mimeType, KisImportExportManager::Direction direction)
{
    KisImportExportFilter *filter =
        KisImportExportManager::filterForMimeType(PkString(mimeType), direction);
    if (!filter) {
        fail(std::string("factory returned null for ") + mimeType);
    }
    delete filter;
}

void requireDynamicType(const char *mimeType,
                        KisImportExportManager::Direction direction,
                        const char *expectedClass)
{
    KisImportExportFilter *filter =
        KisImportExportManager::filterForMimeType(PkString(mimeType), direction);
    if (!filter) {
        fail(std::string("factory returned null for collision ") + mimeType);
    }
    const std::string dynamicName = typeid(*filter).name();
    delete filter;
    if (dynamicName.find(expectedClass) == std::string::npos) {
        fail(std::string("highest-weight collision selected ") + dynamicName +
             " instead of " + expectedClass);
    }
}
#endif

} // namespace

int main()
{
    for (const NamedRegistration &registration : registrations) {
        if (!registration.function) {
            fail(std::string("registration function is absent: ") + registration.name);
        }
        if (!registration.function()) {
            fail(std::string("first registration was rejected: ") + registration.name);
        }

#ifndef TASK6_RED_ONLY
        // Exercise the lower-weight QImageIO WebP factories before the native
        // WebP registrations supersede them (JSON weights 1 and 4 respectively).
        if (registration.function == &registerKisQImageIOExportFilter) {
            requireDynamicType("image/webp", KisImportExportManager::Export,
                               "KisQImageIOExport");
        } else if (registration.function == &registerKisQImageIOImportFilter) {
            requireDynamicType("image/webp", KisImportExportManager::Import,
                               "KisQImageIOImport");
        }
#endif
    }

#ifndef TASK6_RED_ONLY
    // Registration entry points report whether this call appended their
    // entry. This observes the per-entry guard directly rather than through
    // supportedMimeTypes(), whose public projection intentionally removes
    // duplicates.
    for (const NamedRegistration &registration : registrations) {
        if (registration.function()) {
            fail(std::string("duplicate registration was accepted: ") + registration.name);
        }
    }

    requireDynamicType("image/webp", KisImportExportManager::Import, "KisWebPImport");
    requireDynamicType("image/webp", KisImportExportManager::Export, "KisWebPExport");
#endif

#ifndef TASK6_RED_ONLY
    const auto actualImports =
        toSet(KisImportExportManager::supportedMimeTypes(KisImportExportManager::Import));
    const auto actualExports =
        toSet(KisImportExportManager::supportedMimeTypes(KisImportExportManager::Export));
    if (actualImports != expectedSet(expectedImportMimeTypes)) {
        fail("static registry import MIME union differs from the JSON oracle");
    }
    if (actualExports != expectedSet(expectedExportMimeTypes)) {
        fail("static registry export MIME union differs from the JSON oracle");
    }

    for (const char *mimeType : expectedImportMimeTypes) {
        requireFactory(mimeType, KisImportExportManager::Import);
    }
    for (const char *mimeType : expectedExportMimeTypes) {
        requireFactory(mimeType, KisImportExportManager::Export);
    }

    requireDynamicType("image/webp", KisImportExportManager::Import, "KisWebPImport");
    requireDynamicType("image/webp", KisImportExportManager::Export, "KisWebPExport");
#endif
    return 0;
}
