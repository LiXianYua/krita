/*
 *  SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_gif_export.h"

#include <kpluginfactory.h>
#include <KoColorModelStandardIds.h>
#include <KisExportCheckRegistry.h>
#include <KisImportExportManager.h>
#include <KisImportExportBackend.h>
#include <kis_paint_device.h>
#include <kis_image.h>
#include <kis_paint_layer.h>

#include "qgiflibhandler.h"

K_PLUGIN_FACTORY_WITH_JSON(KisGIFExportFactory, "krita_gif_export.json", registerPlugin<KisGIFExport>();)

KisGIFExport::KisGIFExport(QObject *parent, const PkVariantList &) : KisImportExportFilter(parent)
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



#include "kis_gif_export.moc"
