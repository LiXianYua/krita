/*
 *  SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <KisDocument.h>

#include "kis_raw_import.h"
#include "../kis_impex_static_registration.h"
#include <KisImportExportErrorCode.h>

extern "C" KRITAIMPEX_EXPORT bool registerKisRawImportFilter()
{
    static bool registered = false;
    return registerKisImpexFilterOnce(
        registered, {PkString("image/x-krita-raw")}, {}, 1,
        []() -> KisImportExportFilter * { return new KisRawImport(nullptr, PkVariantList()); });
}

KisRawImport::KisRawImport(PkObject *parent, const PkVariantList &)
    : KisImportExportFilter(parent)
{
}

KisRawImport::~KisRawImport()
{
}

KisImportExportErrorCode
KisRawImport::convert(KisDocument *document, PkStream *io, KisPropertiesConfigurationSP configuration)
{
    (void)document;
    (void)io;
    (void)configuration;

    // The available KDcraw API is a Qt wrapper around LibRaw. This target has
    // no native LibRaw development API in the configured toolchain, so keeping
    // KDcraw would hide a Qt dependency. Fail truthfully until a native decoder
    // adapter is available; do not report a successful import without pixels.
    return ImportExportCodes::FormatFeaturesUnsupported;
}
