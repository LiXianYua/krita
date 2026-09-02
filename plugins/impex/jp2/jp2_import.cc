/*
 *  SPDX-FileCopyrightText: 2009 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.1-only
 */

#include <KisDocument.h>

#include "jp2_import.h"
#include "../kis_impex_static_registration.h"
#include <kis_image.h>

#include "jp2_converter.h"

extern "C" KRITAIMPEX_EXPORT bool registerjp2ImportFilter()
{
    static bool registered = false;
    return registerKisImpexFilterOnce(
        registered, {PkString("image/jp2"), PkString("image/jpeg2000"), PkString("image/jpx"), PkString("image/jpeg2000-image"), PkString("image/x-jpeg2000-image")}, {}, 1,
        []() -> KisImportExportFilter * { return new jp2Import(nullptr, PkVariantList()); });
}

jp2Import::jp2Import(PkObject *parent, const PkVariantList &) : KisImportExportFilter(parent)
{
}

jp2Import::~jp2Import()
{
}

KisImportExportErrorCode jp2Import::convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP /*configuration*/)
{
    JP2Converter converter(document);
    if (!io || !io->isReadable()) {
        return ImportExportCodes::NoAccessToRead;
    }
    KisImportExportErrorCode result = converter.buildImage(io);
    if (result.isOk()) {
        document->setCurrentImage(converter.image());
    }
    return result;
}
