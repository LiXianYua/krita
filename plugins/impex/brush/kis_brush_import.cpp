/*
 *  SPDX-FileCopyrightText: 2016 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_brush_import.h"
#include "../kis_impex_static_registration.h"
#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>
#include <KoColorModelStandardIds.h>

#include <KisDocument.h>

#include <kis_transaction.h>
#include <kis_paint_device.h>
#include <kis_image.h>
#include <kis_paint_layer.h>
#include <kis_node.h>
#include <kis_group_layer.h>

#include <kis_gbr_brush.h>
#include <kis_imagepipe_brush.h>
#include <KisAnimatedBrushAnnotation.h>
#include <KisGlobalResourcesInterface.h>

extern "C" KRITAIMPEX_EXPORT void registerKisBrushImportFilter()
{
    static bool registered = false;
    registerKisImpexFilterOnce(
        registered, {PkString("image/x-gimp-brush"), PkString("image/x-gimp-brush-animated")}, {}, 1,
        []() -> KisImportExportFilter * { return new KisBrushImport(nullptr, PkVariantList()); });
}

KisBrushImport::KisBrushImport(PkObject *parent, const PkVariantList &) : KisImportExportFilter(parent)
{
}

KisBrushImport::~KisBrushImport()
{
}


KisImportExportErrorCode KisBrushImport::convert(KisDocument *document, PkStream *io, KisPropertiesConfigurationSP /*configuration*/)
{
    PkSharedPointer<KisColorfulBrush> brush;

    if (mimeType() == "image/x-gimp-brush") {
        brush = PkSharedPointer<KisColorfulBrush>(new KisGbrBrush(filename()));
    }
    else if (mimeType() == "image/x-gimp-brush-animated") {
        brush = PkSharedPointer<KisColorfulBrush>(new KisImagePipeBrush(filename()));
    }
    else {
        return ImportExportCodes::FileFormatIncorrect;
    }

    if (!brush->loadFromDevice(io, KisGlobalResourcesInterface::instance())) {
        return ImportExportCodes::FileFormatIncorrect;
    }

    if (!brush->valid()) {
        return ImportExportCodes::FileFormatIncorrect;;
    }

    const KoColorSpace *colorSpace = 0;
    if (brush->isImageType()) {
        colorSpace = KoColorSpaceRegistry::instance()->rgb8();
        brush->setBrushApplication(IMAGESTAMP);
    }
    else {
        colorSpace = KoColorSpaceRegistry::instance()->colorSpace(GrayAColorModelID.id(), Integer8BitsColorDepthID.id(), "");
    }

    KisImageSP image = new KisImage(document->createUndoStore(), brush->width(), brush->height(), colorSpace, brush->name());
    KisImagePipeBrushSP pipeBrush = brush.dynamicCast<KisImagePipeBrush>();
    if (pipeBrush) {
        PkVector<KisGbrBrushSP> brushes = pipeBrush->brushes();
        for(int i = brushes.size(); i > 0; i--) {
            KisGbrBrushSP subbrush = brushes.at(i - 1);
            const KoColorSpace *subColorSpace = 0;
            if (subbrush->isImageType()) {
                subColorSpace = KoColorSpaceRegistry::instance()->rgb8();
                subbrush->setBrushApplication(IMAGESTAMP);
            }
            else {
                subColorSpace = KoColorSpaceRegistry::instance()->colorSpace(GrayAColorModelID.id(), Integer8BitsColorDepthID.id(), "");
            }
            KisPaintLayerSP layer = new KisPaintLayer(image, image->nextLayerName(), 255, subColorSpace);
            layer->paintDevice()->convertFromQImage(subbrush->brushTipImage(), 0, 0, 0);
            image->addNode(layer, image->rootLayer());
        }
        KisAnnotationSP ann = new KisAnimatedBrushAnnotation(pipeBrush->parasite());
        image->addAnnotation(ann);
    }
    else {
        KisPaintLayerSP layer = new KisPaintLayer(image, image->nextLayerName(), 255, colorSpace);
        layer->paintDevice()->convertFromQImage(brush->brushTipImage(), 0, 0, 0);
        image->addNode(layer, image->rootLayer(), 0);
    }

    document->setCurrentImage(image);
    return ImportExportCodes::OK;

}
