/*
 *  SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_gif_import.h"

#include <kpluginfactory.h>

#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>

#include <kis_transaction.h>
#include <kis_paint_device.h>
#include <kis_image.h>
#include <kis_paint_layer.h>
#include <kis_node.h>
#include <kis_group_layer.h>
#include <KisDocument.h>
#include <KisImportExportAdditionalChecks.h>

#include "qgiflibhandler.h"

K_PLUGIN_FACTORY_WITH_JSON(KisGIFImportFactory, "krita_gif_import.json", registerPlugin<KisGIFImport>();)

KisGIFImport::KisGIFImport(QObject *parent, const PkVariantList &)
    : KisImportExportFilter(parent)
{
}

KisGIFImport::~KisGIFImport()
{
}

KisImportExportErrorCode KisGIFImport::convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration)
{
    (void)configuration;

    PkImage img;
    GifLibCodec handler(io);

    if (!io->isReadable()) {
        return ImportExportCodes::NoAccessToRead;
    }

    if (handler.canRead()) {
        if (!handler.read(&img)) {
            return ImportExportCodes::FileFormatIncorrect;
        }
    } else {
        // handler.canRead() checks for the flag in the file; if it can't read it, maybe the format is incorrect
        return ImportExportCodes::FileFormatIncorrect;
    }

    const KoColorSpace *colorSpace = KoColorSpaceRegistry::instance()->rgb8();
    KisImageSP image = new KisImage(document->createUndoStore(), img.width(), img.height(), colorSpace, "imported from gif");

    KisPaintLayerSP layer = new KisPaintLayer(image, image->nextLayerName(), 255);
    layer->paintDevice()->convertFromQImage(img, 0, 0, 0);
    image->addNode(layer.data(), image->rootLayer().data());

    document->setCurrentImage(image);
    return ImportExportCodes::OK;

}

#include "kis_gif_import.moc"
