/*
 *  SPDX-FileCopyrightText: 2005 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_png_export.h"

#include <QCheckBox>
#include <QSlider>
#include <QApplication>

#include <kpluginfactory.h>

#include <KoColorSpace.h>
#include <KisImportExportManager.h>
#include <KisImportExportErrorCode.h>
#include <KoColorProfile.h>
#include <KoColorModelStandardIds.h>
#include <KoColorSpaceRegistry.h>

#include <KisExportCheckRegistry.h>

#include <kis_properties_configuration.h>
#include <kis_paint_device.h>
#include <KisDocument.h>
#include <kis_image.h>
#include <kis_paint_layer.h>
#include <kis_group_layer.h>
#include <kis_config.h>
#include <kis_meta_data_store.h>
#include <kis_meta_data_filter_registry_model.h>
#include <kis_exif_info_visitor.h>
#include "kis_png_converter.h"
#include <kis_iterator_ng.h>

K_PLUGIN_FACTORY_WITH_JSON(KisPNGExportFactory, "krita_png_export.json", registerPlugin<KisPNGExport>();)

KisPNGExport::KisPNGExport(QObject *parent, const QVariantList &) : KisImportExportFilter(parent)
{
}

KisPNGExport::~KisPNGExport()
{
}

KisImportExportErrorCode KisPNGExport::convert(KisDocument *document, QIODevice *io,  KisPropertiesConfigurationSP configuration)
{
    KisImageSP image = document->savingImage();

    KisPNGOptions options;

    options.alpha = configuration->getBool("alpha", true);
    options.interlace = configuration->getBool("interlaced", false);
    options.compression = configuration->getInt("compression", 3);
    options.tryToSaveAsIndexed = configuration->getBool("indexed", false);
    KoColor c(KoColorSpaceRegistry::instance()->rgb8());
    c.fromQColor(Qt::white);
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

    KisPNGConverter pngConverter(document);

    KisImportExportErrorCode res = pngConverter.buildFile(io, image->bounds(), image->xRes(), image->yRes(), image->projection(), beginIt, endIt, options, eI);
    delete eI;
    dbgFile << " Result =" << res;
    return res;
}

KisPropertiesConfigurationSP KisPNGExport::defaultConfiguration(const QByteArray &, const QByteArray &) const
{
    KisPropertiesConfigurationSP cfg = new KisPropertiesConfiguration();
    cfg->setProperty("alpha", true);
    cfg->setProperty("indexed", false);
    cfg->setProperty("compression", 3);
    cfg->setProperty("interlaced", false);

    KoColor fill_color(KoColorSpaceRegistry::instance()->rgb8());
    fill_color = KoColor();
    fill_color.fromQColor(Qt::white);
    QVariant v;
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
    QList<QPair<KoID, KoID> > supportedColorModels;
    supportedColorModels << QPair<KoID, KoID>()
            << QPair<KoID, KoID>(RGBAColorModelID, Integer8BitsColorDepthID)
            << QPair<KoID, KoID>(RGBAColorModelID, Integer16BitsColorDepthID)
            << QPair<KoID, KoID>(GrayAColorModelID, Integer8BitsColorDepthID)
            << QPair<KoID, KoID>(GrayAColorModelID, Integer16BitsColorDepthID);
    addSupportedColorModels(supportedColorModels, "PNG");
}

#include "kis_png_export.moc"

