/*
 *  SPDX-FileCopyrightText: 2005 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_jpeg_import.h"
#include "../kis_impex_static_registration.h"
#include <KisDocument.h>
#include <kis_image.h>
#include <KisImportExportManager.h>

#include "kis_jpeg_converter.h"

extern "C" KRITAIMPEX_EXPORT void registerKisJPEGImportFilter()
{
    static bool registered = false;
    registerKisImpexFilterOnce(
        registered, {PkString("image/jpeg")}, {}, 1,
        []() -> KisImportExportFilter * { return new KisJPEGImport(nullptr, PkVariantList()); });
}

KisJPEGImport::KisJPEGImport(PkObject *parent, const PkVariantList &) : KisImportExportFilter(parent)
{
}

KisJPEGImport::~KisJPEGImport()
{
}

KisImportExportErrorCode KisJPEGImport::convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP /*configuration*/)
{
    KisJPEGConverter ib(document, batchMode());
    KisImportExportErrorCode result = ib.buildImage(io);
    if (result.isOk()) {
        document->setCurrentImage(ib.image());
    }
    return result;
}
