/*
 *  SPDX-FileCopyrightText: 2007 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_qimageio_import.h"
#include "../kis_impex_static_registration.h"
#include <KisDocument.h>


extern "C" KRITAIMPEX_EXPORT bool registerKisQImageIOImportFilter()
{
    static bool registered = false;
    return registerKisImpexFilterOnce(
        registered, {PkString("image/bmp"), PkString("image/x-xpixmap"), PkString("image/x-xbitmap"), PkString("image/vnd.microsoft.icon"), PkString("image/x-portable-pixmap"), PkString("image/x-portable-graymap"), PkString("image/x-portable-bitmap"), PkString("image/webp")}, {}, 1,
        []() -> KisImportExportFilter * { return new KisQImageIOImport(nullptr, PkVariantList()); });
}

KisQImageIOImport::KisQImageIOImport(PkObject *parent, const PkVariantList &) : KisImportExportFilter(parent)
{
}

KisQImageIOImport::~KisQImageIOImport()
{
}

KisImportExportErrorCode KisQImageIOImport::convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP /*configuration*/)
{
    (void)io;
    document->setErrorMessage(PkString("Generic image I/O formats have no native headless codec"));
    return ImportExportCodes::FormatFeaturesUnsupported;

}
