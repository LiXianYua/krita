/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <KisDocument.h>

#include "ora_converter.h"

#include <KoStore.h>
#include <KoStoreDevice.h>
#include <KoColorSpaceRegistry.h>
#include <kis_group_layer.h>
#include <kis_image.h>
#include <kis_open_raster_stack_load_visitor.h>
#include <kis_open_raster_stack_save_visitor.h>
#include <kis_paint_layer.h>
#include <KisPngCodec.h>
#include "kis_open_raster_load_context.h"
#include "kis_open_raster_save_context.h"

OraConverter::OraConverter(KisDocument *doc)
    : m_doc(doc)
{
}

OraConverter::~OraConverter()
{
}

KisImportExportErrorCode OraConverter::buildImage(PkStream *io)
{
    KoStore* store = KoStore::createStore(
        io, KoStore::Read,
        PkByteArray("image/openraster", sizeof("image/openraster") - 1), KoStore::Zip);
    if (!store) {
        delete store;
        return ImportExportCodes::FileFormatIncorrect;
    }

    KisOpenRasterLoadContext olc(store);
    KisOpenRasterStackLoadVisitor orslv(m_doc->createUndoStore(), &olc);
    orslv.loadImage();
    m_image = orslv.image();

    if (!m_image) {
        delete store;
        return ImportExportCodes::ErrorWhileReading;
    }

    m_activeNodes = orslv.activeNodes();
    delete store;

    return ImportExportCodes::OK;
}

KisImageSP OraConverter::image()
{
    return m_image;
}

vKisNodeSP OraConverter::activeNodes()
{
    return m_activeNodes;
}

KisImportExportErrorCode OraConverter::buildFile(PkStream *io, KisImageSP image, vKisNodeSP activeNodes)
{

    // Open file for writing
    KoStore* store = KoStore::createStore(
        io, KoStore::Write,
        PkByteArray("image/openraster", sizeof("image/openraster") - 1), KoStore::Zip);
    if (!store) {
        delete store;
        return ImportExportCodes::ErrorWhileWriting;
    }

    KisOpenRasterSaveContext osc(store);
    KisOpenRasterStackSaveVisitor orssv(&osc, activeNodes);

    if (!image->rootLayer()->accept(orssv)) {
        delete store;
        return ImportExportCodes::ErrorWhileWriting;
    }

    PkSize previewSize = image->bounds().size();
    previewSize.scale(PkSize(256,256), Qt::KeepAspectRatio);

    PkImage preview = image->convertToQImage(previewSize, 0);

    KisPaintDeviceSP previewDevice = new KisPaintDevice(KoColorSpaceRegistry::instance()->rgb8());
    previewDevice->convertFromQImage(preview, nullptr, 0, 0);
    if (!KisPngCodec::saveDeviceToStore("Thumbnails/thumbnail.png", preview.rect(),
                                        image->xRes(), image->yRes(), previewDevice, store)) {
        delete store;
        return ImportExportCodes::ErrorWhileWriting;
    }

    KisPaintDeviceSP dev = image->projection();
    if (!KisPngCodec::saveDeviceToStore("mergedimage.png", image->bounds(), image->xRes(),
                                        image->yRes(), dev, store)) {
        delete store;
        return ImportExportCodes::ErrorWhileWriting;
    }

    delete store;
    return ImportExportCodes::OK;
}
