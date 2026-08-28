/*
 * SPDX-FileCopyrightText: 2016 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "kra_export.h"
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

#include "kra_converter.h"

class KisExternalLayer;

extern "C" KRITAIMPEX_EXPORT void registerKraExportFilter()
{
    static bool registered = false;
    registerKisImpexFilterOnce(
        registered, {}, {PkString("application/x-krita")}, 1,
        []() -> KisImportExportFilter * { return new KraExport(nullptr, PkVariantList()); });
}

KraExport::KraExport(PkObject *parent, const PkVariantList &) : KisImportExportFilter(parent)
{
}

KraExport::~KraExport()
{
}

KisImportExportErrorCode KraExport::convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP /*configuration*/)
{
    KisImageSP image = document->savingImage();
    KIS_ASSERT_RECOVER_RETURN_VALUE(image, ImportExportCodes::InternalError);

    KraConverter kraConverter(document, updater());
    KisImportExportErrorCode res = kraConverter.buildFile(io, filename(), !document->isAutosaving());
    dbgFile << "KraExport::convert result =" << res;
    return res;
}

void KraExport::initializeCapabilities()
{
    // Kra supports everything, by definition
    KisExportCheckFactory *factory = 0;
    for (const PkString &id : KisExportCheckRegistry::instance()->keys()) {
        factory = KisExportCheckRegistry::instance()->get(id);
        addCapability(factory->create(KisExportCheckBase::SUPPORTED));
    }
}

PkString KraExport::verify(const PkString &fileName) const
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

bool KraExport::exportSupportsGuides() const {
    return true;
}
