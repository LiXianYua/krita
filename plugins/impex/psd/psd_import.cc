/*
 *  SPDX-FileCopyrightText: 2009 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "psd_import.h"
#include "../kis_impex_static_registration.h"
#include <KisDocument.h>
#include <kis_image.h>

#include "psd_loader.h"

extern "C" KRITAIMPEX_EXPORT bool registerpsdImportFilter()
{
    static bool registered = false;
    return registerKisImpexFilterOnce(
        registered, {PkString("image/x-psd"), PkString("image/photoshop"), PkString("image/x-photoshop"), PkString("image/vnd.adobe.photoshop"), PkString("image/x-psb")}, {}, 1,
        []() -> KisImportExportFilter * { return new psdImport(nullptr, PkVariantList()); });
}

psdImport::psdImport(PkObject *parent, const PkVariantList &) : KisImportExportFilter(parent)
{
}

psdImport::~psdImport()
{
}

KisImportExportErrorCode psdImport::convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP /*configuration*/)
{
    PSDLoader ib(document, importUserFeedBackInterface());
    KisImportExportErrorCode result = ib.buildImage(*io);
    if (result.isOk()) {
        document->setCurrentImage(ib.image());
    }
    return result;
}
