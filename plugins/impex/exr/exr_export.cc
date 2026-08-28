/*
 *  SPDX-FileCopyrightText: 2010 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "exr_export.h"

#include <kpluginfactory.h>

#include <KoColorSpaceRegistry.h>
#include <KoColorSpaceConstants.h>
#include <KisImportExportManager.h>
#include <KisExportCheckRegistry.h>

#include <kis_properties_configuration.h>
#include <KisDocument.h>
#include <kis_image.h>
#include <kis_group_layer.h>
#include <kis_paint_device.h>
#include <kis_paint_layer.h>

#include "exr_converter.h"


class KisExternalLayer;

K_PLUGIN_FACTORY_WITH_JSON(ExportFactory, "krita_exr_export.json", registerPlugin<EXRExport>();)

EXRExport::EXRExport(QObject *parent, const PkVariantList &) : KisImportExportFilter(parent)
{
}

EXRExport::~EXRExport()
{
}

KisPropertiesConfigurationSP EXRExport::defaultConfiguration(const PkByteArray &/*from*/, const PkByteArray &/*to*/) const
{
    KisPropertiesConfigurationSP cfg = new KisPropertiesConfiguration();
    cfg->setProperty("flatten", false);
    return cfg;
}

KisImportExportErrorCode EXRExport::convert(KisDocument *document, PkStream */*io*/,  KisPropertiesConfigurationSP configuration)
{
    if (!document || !configuration) {
        return ImportExportCodes::InternalError;
    }

    KisImageSP image = document->savingImage();
    if (!image) {
        return ImportExportCodes::InternalError;
    }

    EXRConverter exrConverter(document, !batchMode());

    KisImportExportErrorCode res;

    if (configuration && configuration->getBool("flatten")) {
        res = exrConverter.buildFile(filename(), image->rootLayer(), true);
    }
    else {
        res = exrConverter.buildFile(filename(), image->rootLayer());
    }

    if (!exrConverter.errorMessage().isEmpty()) {
        document->setErrorMessage(exrConverter.errorMessage());
    }


    dbgFile  << " Result =" << res;
    return res;
}

void EXRExport::initializeCapabilities()
{
    addCapability(KisExportCheckRegistry::instance()->get("NodeTypeCheck/KisGroupLayer")->create(KisExportCheckBase::SUPPORTED));
    addCapability(KisExportCheckRegistry::instance()->get("MultiLayerCheck")->create(KisExportCheckBase::SUPPORTED));
    addCapability(KisExportCheckRegistry::instance()->get("sRGBProfileCheck")->create(KisExportCheckBase::SUPPORTED));
    addCapability(KisExportCheckRegistry::instance()->get("LayerOpacityCheck")->create(KisExportCheckBase::SUPPORTED));

    PkList<std::pair<KoID, KoID> > supportedColorModels;
    supportedColorModels << std::pair<KoID, KoID>()
            << std::pair<KoID, KoID>(RGBAColorModelID, Float16BitsColorDepthID)
            << std::pair<KoID, KoID>(RGBAColorModelID, Float32BitsColorDepthID)
            << std::pair<KoID, KoID>(GrayAColorModelID, Float16BitsColorDepthID)
            << std::pair<KoID, KoID>(GrayAColorModelID, Float32BitsColorDepthID)
            << std::pair<KoID, KoID>(GrayColorModelID, Float16BitsColorDepthID)
            << std::pair<KoID, KoID>(GrayColorModelID, Float32BitsColorDepthID)
            << std::pair<KoID, KoID>(XYZAColorModelID, Float16BitsColorDepthID)
            << std::pair<KoID, KoID>(XYZAColorModelID, Float32BitsColorDepthID);
    addSupportedColorModels(supportedColorModels, "EXR");
}

#include <exr_export.moc>
