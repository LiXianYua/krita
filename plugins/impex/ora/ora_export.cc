/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "ora_export.h"
#include "../kis_impex_static_registration.h"
#include <KoStore.h>
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

#include "ora_converter.h"

class KisExternalLayer;

extern "C" KRITAIMPEX_EXPORT void registerOraExportFilter()
{
    static bool registered = false;
    registerKisImpexFilterOnce(
        registered, {}, {PkString("image/openraster")}, 1,
        []() -> KisImportExportFilter * { return new OraExport(nullptr, PkVariantList()); });
}

OraExport::OraExport(PkObject *parent, const PkVariantList &) : KisImportExportFilter(parent)
{
}

OraExport::~OraExport()
{
}


bool hasShapeLayerChild(KisNodeSP node)
{
    if (!node) return false;

    for (KisNodeSP child : node->childNodes(PkStringList(), KoProperties())) {
        if (child->inherits("KisShapeLayer")
                || child->inherits("KisGeneratorLayer")
                || child->inherits("KisCloneLayer")) {
            return true;
        }
        else {
            if (hasShapeLayerChild(child)) {
                return true;
            }
        }
    }
    return false;
}

KisImportExportErrorCode OraExport::convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP /*configuration*/)
{
    KisImageSP image = document->savingImage();
    if (!image) {
        return ImportExportCodes::InternalError;
    }
    OraConverter oraConverter(document);

    KisImportExportErrorCode res = oraConverter.buildFile(io, image, {document->preActivatedNode()});
    return res;
}

void OraExport::initializeCapabilities()
{
    addCapability(KisExportCheckRegistry::instance()->get("MultiLayerCheck")->create(KisExportCheckBase::SUPPORTED));
    addCapability(KisExportCheckRegistry::instance()->get("NodeTypeCheck/KisGroupLayer")->create(KisExportCheckBase::SUPPORTED));
    addCapability(KisExportCheckRegistry::instance()->get("NodeTypeCheck/KisAdjustmentLayer")->create(KisExportCheckBase::SUPPORTED));
    addCapability(KisExportCheckRegistry::instance()->get("sRGBProfileCheck")->create(KisExportCheckBase::SUPPORTED));
    addCapability(KisExportCheckRegistry::instance()->get("ColorModelHomogenousCheck")->create(KisExportCheckBase::SUPPORTED));
    addCapability(KisExportCheckRegistry::instance()->get("LayerOpacityCheck")->create(KisExportCheckBase::SUPPORTED));
    PkList<std::pair<KoID, KoID> > supportedColorModels;
    supportedColorModels << std::pair<KoID, KoID>()
            << std::pair<KoID, KoID>(RGBAColorModelID, Integer8BitsColorDepthID)
            << std::pair<KoID, KoID>(RGBAColorModelID, Integer16BitsColorDepthID)
            << std::pair<KoID, KoID>(GrayAColorModelID, Integer8BitsColorDepthID)
            << std::pair<KoID, KoID>(GrayAColorModelID, Integer16BitsColorDepthID);
    addSupportedColorModels(supportedColorModels, "OpenRaster");
}

PkString OraExport::verify(const PkString &fileName) const
{
    PkString error = KisImportExportFilter::verify(fileName);
    if (error.isEmpty()) {
        return KisImportExportFilter::verifyZiPBasedFiles(fileName,
                                                          PkStringList()
                                                          << "mimetype"
                                                          << "stack.xml"
                                                          << "mergedimage.png");
    }

    return error;
}
