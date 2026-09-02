/*
 *  SPDX-FileCopyrightText: 2010 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <KisDocument.h>

#include "exr_import.h"
#include "../kis_impex_static_registration.h"
#include <KisImportExportManager.h>

#include <kis_image.h>

#include "exr_converter.h"

extern "C" KRITAIMPEX_EXPORT bool registerexrImportFilter()
{
    static bool registered = false;
    return registerKisImpexFilterOnce(
        registered, {PkString("image/x-exr"), PkString("application/x-extension-exr")}, {}, 1,
        []() -> KisImportExportFilter * { return new exrImport(nullptr, PkVariantList()); });
}

exrImport::exrImport(PkObject *parent, const PkVariantList &) : KisImportExportFilter(parent)
{
}

exrImport::~exrImport()
{
}

KisImportExportErrorCode exrImport::convert(KisDocument *document, PkStream */*io*/,  KisPropertiesConfigurationSP /*configuration*/)
{
    EXRConverter ib(document, !batchMode());
    KisImportExportErrorCode result = ib.buildImage(filename());
    if (result.isOk()) {
        document->setCurrentImage(ib.image());
        if (!ib.errorMessage().isEmpty()) {
            document->setWarningMessage(ib.errorMessage());
        }
    }
    return result;
}
