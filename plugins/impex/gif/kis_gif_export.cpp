/*
 *  SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_gif_export.h"
#include "../kis_impex_static_registration.h"
#include <KoColorModelStandardIds.h>
#include <KisExportCheckRegistry.h>
#include <KisImportExportManager.h>
#include <KisImportExportBackend.h>
#include <kis_paint_device.h>
#include <kis_image.h>
#include <kis_paint_layer.h>

#include "qgiflibhandler.h"

extern "C" KRITAIMPEX_EXPORT bool registerKisGIFExportFilter()
{
    static bool registered = false;
    return registerKisImpexFilterOnce(
        registered, {}, {PkString("image/gif")}, 1,
        []() -> KisImportExportFilter * { return new KisGIFExport(nullptr, PkVariantList()); });
}

KisGIFExport::KisGIFExport(PkObject *parent, const PkVariantList &) : KisImportExportFilter(parent)
{
}

KisGIFExport::~KisGIFExport()
{
}

KisImportExportErrorCode KisGIFExport::convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration)
{
    (void)configuration;
    KisImageSP savingImage = kisImportExportSavingImage(document);
    PkRect rc = savingImage->bounds();
    PkImage image = savingImage->projection()->convertToQImage(0, 0, 0, rc.width(), rc.height(), KoColorConversionTransformation::internalRenderingIntent(), KoColorConversionTransformation::internalConversionFlags());

    GifLibCodec handler(io);
    bool result = handler.write(image);
    if (!result) {
       KIS_ASSERT_RECOVER_RETURN_VALUE(true, ImportExportCodes::InternalError);
       return ImportExportCodes::InternalError;
    }
    return ImportExportCodes::OK;
}

void KisGIFExport::initializeCapabilities()
{

    PkList<std::pair<KoID, KoID> > supportedColorModels;
    supportedColorModels << std::pair<KoID, KoID>()
            << std::pair<KoID, KoID>(RGBAColorModelID, Integer8BitsColorDepthID);
    addSupportedColorModels(supportedColorModels, "GIF");
}
