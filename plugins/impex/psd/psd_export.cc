/*
 *  SPDX-FileCopyrightText: 2009 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "psd_export.h"
#include "../kis_impex_static_registration.h"
#include <KisExportCheckRegistry.h>
#include <KisImportExportManager.h>
#include <ImageSizeCheck.h>
#include <KoColorSpaceRegistry.h>
#include <KoColorSpaceConstants.h>

#include <KisDocument.h>
#include <kis_image.h>
#include <kis_group_layer.h>
#include <kis_paint_layer.h>
#include <kis_paint_device.h>

#include "psd_saver.h"

class KisExternalLayer;

extern "C" KRITAIMPEX_EXPORT bool registerpsdExportFilter()
{
    static bool registered = false;
    return registerKisImpexFilterOnce(
        registered, {}, {PkString("image/x-psd"), PkString("image/photoshop"), PkString("image/x-photoshop"), PkString("image/vnd.adobe.photoshop")}, 1,
        []() -> KisImportExportFilter * { return new psdExport(nullptr, PkVariantList()); });
}

psdExport::psdExport(PkObject *parent, const PkVariantList &) : KisImportExportFilter(parent)
{
}

psdExport::~psdExport()
{
}

KisImportExportErrorCode psdExport::convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP /*configuration*/)
{
    PSDSaver psdSaver(document);
    return psdSaver.buildFile(*io);
}

void psdExport::initializeCapabilities()
{
    addCapability(KisExportCheckRegistry::instance()->get("PSDLayerStyleCheck")->create(KisExportCheckBase::SUPPORTED));
    addCapability(KisExportCheckRegistry::instance()->get("NodeTypeCheck/KisGroupLayer")->create(KisExportCheckBase::SUPPORTED));
    addCapability(KisExportCheckRegistry::instance()->get("NodeTypeCheck/KisGeneratorLayer")->create(KisExportCheckBase::SUPPORTED));
    addCapability(KisExportCheckRegistry::instance()->get("NodeTypeCheck/KisShapeLayer")->create(KisExportCheckBase::SUPPORTED));
    addCapability(KisExportCheckRegistry::instance()->get("MultiLayerCheck")->create(KisExportCheckBase::SUPPORTED));
    addCapability(KisExportCheckRegistry::instance()->get("sRGBProfileCheck")->create(KisExportCheckBase::SUPPORTED));
    addCapability(KisExportCheckRegistry::instance()->get("MultiLayerCheck")->create(KisExportCheckBase::SUPPORTED));
    addCapability(KisExportCheckRegistry::instance()->get("NodeTypeCheck/KisTransparencyMask")->create(KisExportCheckBase::SUPPORTED));
    addCapability(KisExportCheckRegistry::instance()->get("ColorModelHomogenousCheck")->create(KisExportCheckBase::UNSUPPORTED, PkString("Your image contains one or more layers with a color model that is different from the image.")));
    addCapability(KisExportCheckRegistry::instance()->get("LayerOpacityCheck")->create(KisExportCheckBase::SUPPORTED));

    ImageSizeCheckFactory *factory = dynamic_cast<ImageSizeCheckFactory*>(KisExportCheckRegistry::instance()->get("ImageSizeCheck"));
    if (factory) {
        addCapability(factory->create(30000, 30000, KisExportCheckBase::SUPPORTED));
    }

    PkList<std::pair<KoID, KoID> > supportedColorModels;
    supportedColorModels << std::pair<KoID, KoID>()
            << std::pair<KoID, KoID>(RGBAColorModelID, Integer8BitsColorDepthID)
            << std::pair<KoID, KoID>(RGBAColorModelID, Integer16BitsColorDepthID)
//            << std::pair<KoID, KoID>(RGBAColorModelID, Float16BitsColorDepthID)
//            << std::pair<KoID, KoID>(RGBAColorModelID, Float32BitsColorDepthID)
            << std::pair<KoID, KoID>(GrayAColorModelID, Integer8BitsColorDepthID)
            << std::pair<KoID, KoID>(GrayAColorModelID, Integer16BitsColorDepthID)
            << std::pair<KoID, KoID>(CMYKAColorModelID, Integer8BitsColorDepthID)
            << std::pair<KoID, KoID>(CMYKAColorModelID, Integer16BitsColorDepthID)
            << std::pair<KoID, KoID>(LABAColorModelID, Integer8BitsColorDepthID)
            << std::pair<KoID, KoID>(LABAColorModelID, Integer16BitsColorDepthID);
    addSupportedColorModels(supportedColorModels, "PSD");

    addCapability(KisExportCheckRegistry::instance()->get("FillLayerTypeCheck/color")->create(KisExportCheckBase::SUPPORTED));
    addCapability(KisExportCheckRegistry::instance()->get("FillLayerTypeCheck/pattern")->create(KisExportCheckBase::SUPPORTED));
    addCapability(KisExportCheckRegistry::instance()->get("FillLayerTypeCheck/gradient")->create(KisExportCheckBase::SUPPORTED));

    addCapability(KisExportCheckRegistry::instance()->get("ShapeLayerTypeCheck/KoPathShape")->create(KisExportCheckBase::SUPPORTED));
    addCapability(KisExportCheckRegistry::instance()->get("ShapeLayerTypeCheck/KoPathShape/RectangleShape")->create(KisExportCheckBase::SUPPORTED));
    addCapability(KisExportCheckRegistry::instance()->get("ShapeLayerTypeCheck/KoPathShape/EllipseShape")->create(KisExportCheckBase::SUPPORTED));
    addCapability(KisExportCheckRegistry::instance()->get("ShapeLayerTypeCheck/KoPathShape/StarShape")->create(KisExportCheckBase::SUPPORTED));
    addCapability(KisExportCheckRegistry::instance()->get("ShapeLayerTypeCheck/KoShapeGroup")->create(KisExportCheckBase::SUPPORTED));

    const PkString textShapeWarning = PkString("While text shapes can be saved to psd, only basic features are supported. Advanced features, like text-on-path and opentype features will not be saved.");
    addCapability(KisExportCheckRegistry::instance()->get("ShapeLayerTypeCheck/KoSvgTextShapeID")->create(KisExportCheckBase::PARTIALLY, textShapeWarning));
}

bool psdExport::exportSupportsGuides() const
{
    return true;
}
