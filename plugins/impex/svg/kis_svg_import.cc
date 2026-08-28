/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_svg_import.h"
#include "../kis_impex_static_registration.h"
#include "svg_import_policy.h"
extern "C" KRITAIMPEX_EXPORT void registerKisSVGImportFilter()
{
    static bool registered = false;
    registerKisImpexFilterOnce(
        registered, {PkString("image/svg+xml")}, {}, 1,
        []() -> KisImportExportFilter * { return new KisSVGImport(nullptr, PkVariantList()); });
}

KisSVGImport::KisSVGImport(PkObject *parent, const PkVariantList &) : KisImportExportFilter(parent)
{
}

KisSVGImport::~KisSVGImport()
{
}

KisImportExportErrorCode KisSVGImport::convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration)
{
    (void)document;
    (void)io;
    (void)configuration;

    // The only available downstream shape parser still exposes Qt stream,
    // string, geometry, and container types outside this task's ownership.
    // Keep the 100 PPI policy seam for a future toolkit-free parser, but do not
    // claim an import succeeded without producing an image.
    (void)deterministicSvgImportPolicy();
    return ImportExportCodes::FormatFeaturesUnsupported;
}
