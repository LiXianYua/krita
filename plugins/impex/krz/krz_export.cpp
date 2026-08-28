/*
 * SPDX-FileCopyrightText: 2016 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "krz_export.h"
#include "../kis_impex_static_registration.h"
#include <KisImportExportManager.h>
#include <KoColorModelStandardIds.h>
#include <KoColorSpace.h>

#include <KisExportCheckRegistry.h>
#include <KisDocument.h>
#include <kis_image.h>
#include <kis_node.h>
#include <kis_group_layer.h>
#include <kis_paint_layer.h>
#include <kis_shape_layer.h>
#include <KoProperties.h>
#include <kis_image_config.h>
#include "kra_converter.h"

class KisExternalLayer;

extern "C" KRITAIMPEX_EXPORT void registerKrzExportFilter()
{
    static bool registered = false;
    registerKisImpexFilterOnce(
        registered, {}, {PkString("application/x-krita-archive")}, 1,
        []() -> KisImportExportFilter * { return new KrzExport(nullptr, PkVariantList()); });
}

KrzExport::KrzExport(PkObject *parent, const PkVariantList &)
    : KisImportExportFilter(parent)
{
}

KrzExport::~KrzExport()
{
}

KisImportExportErrorCode KrzExport::convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP /*configuration*/)
{
    KisImageSP image = document->savingImage();
    KIS_ASSERT_RECOVER_RETURN_VALUE(image, ImportExportCodes::InternalError);

    KisImageConfig cfg(true);
    bool compress = cfg.compressKra();
    cfg.setCompressKra(true);
    KraConverter krzConverter(document, updater());
    KisImportExportErrorCode res = krzConverter.buildFile(io, filename(), false);
    cfg.setCompressKra(compress);
    dbgFile << "KrzExport::convert result =" << res;
    return res;
}

void KrzExport::initializeCapabilities()
{
    // Kra supports everything, by definition
    KisExportCheckFactory *factory = 0;
    for (const PkString &id : KisExportCheckRegistry::instance()->keys()) {
        factory = KisExportCheckRegistry::instance()->get(id);
        addCapability(factory->create(KisExportCheckBase::SUPPORTED));
    }
}

PkString KrzExport::verify(const PkString &fileName) const
{
    PkString error = KisImportExportFilter::verify(fileName);
    if (error.isEmpty()) {
        return KisImportExportFilter::verifyZiPBasedFiles(fileName,
                                                          PkStringList()
                                                          << "mimetype"
                                                          << "documentinfo.xml"
                                                          << "maindoc.xml"
                                                          << "preview.png");
    }
    return error;
}
