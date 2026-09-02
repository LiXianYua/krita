/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <KisDocument.h>

#include "ora_import.h"
#include "../kis_impex_static_registration.h"
#include <kis_image.h>

#include "ora_converter.h"

extern "C" KRITAIMPEX_EXPORT bool registerOraImportFilter()
{
    static bool registered = false;
    return registerKisImpexFilterOnce(
        registered, {PkString("image/openraster")}, {}, 1,
        []() -> KisImportExportFilter * { return new OraImport(nullptr, PkVariantList()); });
}

OraImport::OraImport(PkObject *parent, const PkVariantList &) : KisImportExportFilter(parent)
{
}

OraImport::~OraImport()
{
}

KisImportExportErrorCode OraImport::convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP /*configuration*/)
{
    OraConverter oraConverter(document);
    KisImportExportErrorCode result = oraConverter.buildImage(io);
    if (result.isOk()) {
        KisNodeSP preActivatedNode = !oraConverter.activeNodes().isEmpty() ? oraConverter.activeNodes().first() : nullptr;
        document->setCurrentImage(oraConverter.image(), true, preActivatedNode);
    }
    return result;
}
