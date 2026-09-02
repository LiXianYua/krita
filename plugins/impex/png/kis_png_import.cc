/*
 *  SPDX-FileCopyrightText: 2005 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <KisDocument.h>

#include "kis_png_import.h"
#include "../kis_impex_static_registration.h"
#include <KisImportExportManager.h>

#include <KisPngCodec.h>
#include <kis_image.h>

#include "kis_png_document_context.h"
#include "kis_png_import_profile_policy.h"

extern "C" KRITAIMPEX_EXPORT bool registerKisPNGImportFilter()
{
    static bool registered = false;
    return registerKisImpexFilterOnce(
        registered, {PkString("image/png"), PkString("application/x-krita-paintoppreset")}, {}, 1,
        []() -> KisImportExportFilter * { return new KisPNGImport(nullptr, PkVariantList()); });
}

KisPNGImport::KisPNGImport(PkObject *parent, const PkVariantList &) : KisImportExportFilter(parent)
{
}

KisPNGImport::~KisPNGImport()
{
}

KisImportExportErrorCode KisPNGImport::convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP /*configuration*/)
{
    KisPngDocumentContext documentContext(document);
    KisPngImportProfileDesktopPolicy profilePolicy(batchMode());
    KisPngCodec codec(KisPngCodecContext {
        document ? &documentContext : nullptr,
        &profilePolicy
    });
    KisImportExportErrorCode res = codec.buildImage(io);
    if (res.isOk()){
        document->setCurrentImage(codec.image());
    }
    return res;

}
