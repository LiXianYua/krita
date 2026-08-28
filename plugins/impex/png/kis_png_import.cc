/*
 *  SPDX-FileCopyrightText: 2005 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_png_import.h"

#include <kpluginfactory.h>

#include <KisImportExportManager.h>

#include <KisDocument.h>
#include <KisPngCodec.h>
#include <kis_image.h>

#include "kis_png_document_context.h"
#include "kis_png_import_profile_policy.h"

K_PLUGIN_FACTORY_WITH_JSON(PNGImportFactory, "krita_png_import.json", registerPlugin<KisPNGImport>();)

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

#include <kis_png_import.moc>
