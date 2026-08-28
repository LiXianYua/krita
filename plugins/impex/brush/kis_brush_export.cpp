/*
 *  SPDX-FileCopyrightText: 2016 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_brush_export.h"

#include <PkMemoryStream.h>

#include <memory>

#include <KoProperties.h>
#include <kpluginfactory.h>
#include <KisExportCheckRegistry.h>
#include <kis_paint_device.h>
#include <kis_image.h>
#include <KisImportExportBackend.h>
#include <kis_paint_layer.h>
#include <kis_gbr_brush.h>
#include <kis_imagepipe_brush.h>
#include <kis_pipebrush_parasite.h>
#include <KisAnimatedBrushAnnotation.h>
#include <KisImportExportManager.h>

struct KisBrushExportOptions {
    qreal spacing;
    bool mask;
    int brushStyle;
    int dimensions;
    qint32 ranks[KisPipeBrushParasite::MaxDim];
    qint32 selectionModes[KisPipeBrushParasite::MaxDim];
    PkString name;
};


K_PLUGIN_FACTORY_WITH_JSON(KisBrushExportFactory, "krita_brush_export.json", registerPlugin<KisBrushExport>();)

KisBrushExport::KisBrushExport(PkObject *parent, const PkVariantList &) : KisImportExportFilter(parent)
{
}

KisBrushExport::~KisBrushExport()
{
}

KisImportExportErrorCode KisBrushExport::convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration)
{
    KisImageSP image = kisImportExportSavingImage(document);
    KIS_ASSERT_RECOVER_RETURN_VALUE(image, ImportExportCodes::InternalError);

    if (!configuration) {
        configuration = defaultConfiguration();
    }

// XXX: Loading the parasite itself was commented out -- needs investigation
//    KisAnnotationSP annotation = image->annotation("ImagePipe Parasite");
//    KisPipeBrushParasite parasite;
//    if (annotation) {
//        PkMemoryStream buf(const_cast<PkByteArray*>(&annotation->annotation()));
//        buf.open(PkMemoryStream::ReadOnly);
//        parasite.loadFromDevice(&buf);
//        buf.close();
//    }

    KisBrushExportOptions exportOptions;

    exportOptions.spacing = configuration->getDouble("spacing");
    if (!configuration->getString("name").isEmpty()) {
        exportOptions.name = configuration->getString("name");
    }
    else {
        exportOptions.name = image->objectName();
    }

    exportOptions.mask = configuration->getBool("mask");
    exportOptions.brushStyle = configuration->getInt("brushStyle");
    exportOptions.dimensions = configuration->getInt("dimensions");

    for (int i = 0; i < KisPipeBrushParasite::MaxDim; ++i) {
        const PkString suffix = PkString("%1").arg(i);
        exportOptions.selectionModes[i] = configuration->getInt(PkString("selectionMode") + suffix);
        exportOptions.ranks[i] = configuration->getInt(PkString("rank") + suffix);
    }

    std::unique_ptr<KisGbrBrush> brush;
    if (mimeType() == "image/x-gimp-brush") {
        brush.reset(new KisGbrBrush(filename()));
    }
    else if (mimeType() == "image/x-gimp-brush-animated") {
        brush.reset(new KisImagePipeBrush(filename()));
    }
    else {
        return ImportExportCodes::FileFormatIncorrect;
    }

    PkRect rc = image->bounds();

    brush->setSpacing(exportOptions.spacing);

    KisImagePipeBrush *pipeBrush = dynamic_cast<KisImagePipeBrush*>(brush.get());
    if (pipeBrush) {
        // Create parasite. XXX: share with KisCustomBrushWidget
        PkVector< PkVector<KisPaintDevice*> > devices;
        devices.push_back(PkVector<KisPaintDevice*>());

        KoProperties properties;
        properties.setProperty("visible", true);
        PkList<KisNodeSP> layers =
            image->root()->childNodes(PkStringList({PkString("KisLayer")}), properties);

        for (const KisNodeSP &node : layers) {
            // push_front to behave exactly as gimp for gih creation
            devices[0].push_front(node->projection().data());
        }

        PkVector<KisParasite::SelectionMode > modes;

        for (int i = 0; i < KisPipeBrushParasite::MaxDim; ++i) {
            switch (exportOptions.selectionModes[i]) {
            case 0: modes.push_back(KisParasite::Constant); break;
            case 1: modes.push_back(KisParasite::Random); break;
            case 2: modes.push_back(KisParasite::Incremental); break;
            case 3: modes.push_back(KisParasite::Pressure); break;
            case 4: modes.push_back(KisParasite::Angular); break;
            case 5: modes.push_back(KisParasite::Velocity); break;
            default: modes.push_back(KisParasite::Incremental);
            }
        }

        KisPipeBrushParasite parasite;

        parasite.dim = exportOptions.dimensions;
        parasite.ncells = devices.at(0).count();

        int maxRanks = 0;
        for (int i = 0; i < KisPipeBrushParasite::MaxDim; ++i) {
            // ### This can mask some bugs, be careful here in the future
            parasite.rank[i] = exportOptions.ranks[i];
            parasite.selection[i] = modes.at(i);
            maxRanks += exportOptions.ranks[i];
        }

        if (maxRanks > layers.count()) {
            return ImportExportCodes::FileFormatIncorrect;
        }
        // XXX needs movement!
        parasite.setBrushesCount();
        pipeBrush->setParasite(parasite);
        pipeBrush->setDevices(devices, rc.width(), rc.height());
    }
    else {
        if (exportOptions.mask) {
            PkImage convertedImage = image->projection()->convertToQImage(0, 0, 0, rc.width(), rc.height(), KoColorConversionTransformation::internalRenderingIntent(), KoColorConversionTransformation::internalConversionFlags());
            brush->setImage(convertedImage);
            brush->setBrushTipImage(convertedImage);
        } else {
            brush->initFromPaintDev(image->projection(),0,0,rc.width(), rc.height());
        }
    }

    brush->setName(exportOptions.name);
    // brushes are created after devices are loaded, call mask mode after that
    brush->setBrushApplication(exportOptions.mask ? ALPHAMASK : IMAGESTAMP);
    brush->setWidth(rc.width());
    brush->setHeight(rc.height());

    if (brush->saveToDevice(io)) {
        return ImportExportCodes::OK;
    }
    else {
        return ImportExportCodes::Failure;
    }
}

KisPropertiesConfigurationSP KisBrushExport::defaultConfiguration(const PkByteArray &/*from*/, const PkByteArray &/*to*/) const
{
    KisPropertiesConfigurationSP cfg = new KisPropertiesConfiguration();
    cfg->setProperty("spacing", 1.0);
    cfg->setProperty("name", "");
    cfg->setProperty("mask", true);
    cfg->setProperty("brushStyle", 0);
    cfg->setProperty("dimensions", 1);

    for (int i = 0; i < KisPipeBrushParasite::MaxDim; ++i) {
        const PkString suffix = PkString("%1").arg(i);
        cfg->setProperty(PkString("selectionMode") + suffix, 2);
        cfg->getInt(PkString("rank") + suffix, 0);
    }
    return cfg;
}

void KisBrushExport::initializeCapabilities()
{
    PkList<std::pair<KoID, KoID> > supportedColorModels;
    supportedColorModels << std::pair<KoID, KoID>()
            << std::pair<KoID, KoID>(RGBAColorModelID, Integer8BitsColorDepthID)
            << std::pair<KoID, KoID>(GrayAColorModelID, Integer8BitsColorDepthID);
    addSupportedColorModels(supportedColorModels, "Gimp Brushes");
    if (mimeType() == "image/x-gimp-brush-animated") {
        addCapability(KisExportCheckRegistry::instance()->get("MultiLayerCheck")->create(KisExportCheckBase::SUPPORTED));
        addCapability(KisExportCheckRegistry::instance()->get("LayerOpacityCheck")->create(KisExportCheckBase::SUPPORTED));
    }
}


#include "kis_brush_export.moc"
