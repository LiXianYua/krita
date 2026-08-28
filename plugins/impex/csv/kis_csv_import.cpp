/*
 *  SPDX-FileCopyrightText: 2016 Laszlo Fazekas <mneko@freemail.hu>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_csv_import.h"
#include "../kis_impex_static_registration.h"
#include <KisImportExportManager.h>

#include <KisDocument.h>
#include <kis_image.h>

#include "csv_loader.h"

extern "C" KRITAIMPEX_EXPORT void registerKisCSVImportFilter()
{
    static bool registered = false;
    registerKisImpexFilterOnce(
        registered, {PkString("text/csv")}, {}, 1,
        []() -> KisImportExportFilter * { return new KisCSVImport(nullptr, PkVariantList()); });
}

KisCSVImport::KisCSVImport(PkObject *parent, const PkVariantList &) : KisImportExportFilter(parent)
{
}

KisCSVImport::~KisCSVImport()
{
}

KisImportExportErrorCode KisCSVImport::convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP /*configuration*/)
{
    CSVLoader ib(document, batchMode());
    KisImportExportErrorCode result = ib.buildAnimation(io, filename());
    if (result.isOk()) {
        document->setCurrentImage(ib.image());
    }
    return result;
}
