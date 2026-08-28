/*
 *  SPDX-FileCopyrightText: 2005 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_png_export.h"
#include "../kis_impex_static_registration.h"
#include <KoColorSpace.h>
#include <KisImportExportManager.h>
#include <KisImportExportErrorCode.h>
#include <KoColorProfile.h>
#include <KoColorModelStandardIds.h>
#include <KoColorSpaceRegistry.h>

#include <KisExportCheckRegistry.h>
#include <KisPngCodec.h>

#include <kis_properties_configuration.h>
#include <kis_paint_device.h>
#include <KisDocument.h>
#include <kis_image.h>
#include <kis_paint_layer.h>
#include <kis_group_layer.h>
#include <kis_meta_data_store.h>
#include <kis_meta_data_filter_registry_model.h>
#include <kis_exif_info_visitor.h>
#include "kis_png_document_context.h"
#include <kis_iterator_ng.h>

extern "C" KRITAIMPEX_EXPORT void registerKisPNGExportFilter()
{
    static bool registered = false;
    registerKisImpexFilterOnce(
        registered, {}, {PkString("image/png"), PkString("application/x-krita-paintoppreset")}, 1,
        []() -> KisImportExportFilter * { return new KisPNGExport(nullptr, PkVariantList()); });
}

KisPNGExport::KisPNGExport(PkObject *parent, const PkVariantList &) : KisImportExportFilter(parent)
{
}

KisPNGExport::~KisPNGExport()
{
}

KisImportExportErrorCode KisPNGExport::convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration)
{
    KisImageSP image = document->savingImage();

    KisPNGOptions options;

    options.alpha = configuration->getBool("alpha", true);
    options.interlace = configuration->getBool("interlaced", false);
    options.compression = configuration->getInt("compression", 3);
    options.tryToSaveAsIndexed = configuration->getBool("indexed", false);
    KoColor c(KoColorSpaceRegistry::instance()->rgb8());
    c.fromQColor(PkColor(255, 255, 255));
    options.transparencyFillColor = configuration->getColor("transparencyFillcolor", c).toQColor();
    options.saveSRGBProfile = configuration->getBool("saveSRGBProfile", false);
    options.forceSRGB = configuration->getBool("forceSRGB", true);
    options.storeAuthor = configuration->getBool("storeAuthor", false);
    options.storeMetaData = configuration->getBool("storeMetaData", false);
    options.saveAsHDR = configuration->getBool("saveAsHDR", false);
    options.downsample = configuration->getBool("downsample", false);

    vKisAnnotationSP_it beginIt = image->beginAnnotations();
    vKisAnnotationSP_it endIt = image->endAnnotations();

    KisExifInfoVisitor eIV;
    eIV.visit(image->rootLayer().data());
    KisMetaData::Store *eI = 0;
    if (eIV.metaDataCount() == 1) {
        eI = eIV.exifInfo();
    }
    if (eI) {
        KisMetaData::Store* copy = new KisMetaData::Store(*eI);
        eI = copy;
    }

    KisPngDocumentContext documentContext(document);
    KisPngCodec codec(KisPngCodecContext {
        document ? &documentContext : nullptr,
        nullptr
    });

    KisImportExportErrorCode res = codec.buildFile(io, image->bounds(), image->xRes(), image->yRes(), image->projection(), beginIt, endIt, options, eI);
    delete eI;
    dbgFile << " Result =" << res;
    return res;
}

KisPropertiesConfigurationSP KisPNGExport::defaultConfiguration(const PkByteArray &, const PkByteArray &) const
{
    KisPropertiesConfigurationSP cfg = new KisPropertiesConfiguration();
    cfg->setProperty("alpha", true);
    cfg->setProperty("indexed", false);
    cfg->setProperty("compression", 3);
    cfg->setProperty("interlaced", false);

    KoColor fill_color(KoColorSpaceRegistry::instance()->rgb8());
    fill_color = KoColor();
    fill_color.fromQColor(PkColor(255, 255, 255));
    PkVariant v;
    v.setValue(fill_color);

    cfg->setProperty("transparencyFillcolor", v);
    cfg->setProperty("saveSRGBProfile", false);
    cfg->setProperty("forceSRGB", true);
    cfg->setProperty("saveAsHDR", false);
    cfg->setProperty("storeMetaData", false);
    cfg->setProperty("storeAuthor", false);
    cfg->setProperty("downsample", false);
    return cfg;
}

void KisPNGExport::initializeCapabilities()
{
    addCapability(KisExportCheckRegistry::instance()->get("sRGBProfileCheck")->create(KisExportCheckBase::SUPPORTED));
    PkList<std::pair<KoID, KoID> > supportedColorModels;
    supportedColorModels << std::pair<KoID, KoID>()
            << std::pair<KoID, KoID>(RGBAColorModelID, Integer8BitsColorDepthID)
            << std::pair<KoID, KoID>(RGBAColorModelID, Integer16BitsColorDepthID)
            << std::pair<KoID, KoID>(GrayAColorModelID, Integer8BitsColorDepthID)
            << std::pair<KoID, KoID>(GrayAColorModelID, Integer16BitsColorDepthID);
    addSupportedColorModels(supportedColorModels, "PNG");
}
