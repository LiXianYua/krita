
/*
 *  SPDX-FileCopyrightText: 2006 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_pdf_import.h"
#include "../kis_impex_static_registration.h"
#include "pdf_import_policy.h"

#include <PkImage.h>

#include <cstring>

// KDE's headers
#include <kis_debug.h>
#include <kis_paint_device.h>
// calligra's headers
#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>

// krita's headers
#include <KisDocument.h>
#include <kis_image.h>
#include <kis_paint_layer.h>

// plugins's headers
#include <KisImportExportErrorCode.h>

extern "C" KRITAIMPEX_EXPORT void registerKisPDFImportFilter()
{
    static bool registered = false;
    registerKisImpexFilterOnce(
        registered, {PkString("application/pdf")}, {}, 1,
        []() -> KisImportExportFilter * { return new KisPDFImport(nullptr, PkVariantList()); });
}

KisPDFImport::KisPDFImport(PkObject *parent, const PkVariantList &)
    : KisImportExportFilter(parent)
{
}

KisPDFImport::~KisPDFImport()
{
}

KisImportExportErrorCode KisPDFImport::convert(KisDocument *document,
                                               PkStream *io,
                                               KisPropertiesConfigurationSP /*configuration*/)
{
    if (!document || !io) {
        return ImportExportCodes::InternalError;
    }

    const PkByteArray bytes = io->readAll();
    PdfRaster raster;
    constexpr double resolution = 300.0;
    if (!renderPdfFirstPage(bytes.constData(), static_cast<std::size_t>(bytes.size()),
                            resolution, raster)) {
        return ImportExportCodes::FileFormatIncorrect;
    }

    PkImage rendered(raster.width, raster.height, PkImage::Format_ARGB32);
    if (rendered.isNull() || rendered.bytesPerLine() < raster.width * 4) {
        return ImportExportCodes::InsufficientMemory;
    }
    for (int y = 0; y < raster.height; ++y) {
        std::memcpy(rendered.scanLine(y),
                    raster.argb.data() + static_cast<std::size_t>(y) * raster.stride,
                    static_cast<std::size_t>(raster.width) * 4);
    }

    // Create the krita image
    const KoColorSpace* cs = KoColorSpaceRegistry::instance()->rgb8();
    KisImageSP image = new KisImage(document->createUndoStore(), raster.width, raster.height,
                                    cs, "built image");
    image->setResolution(resolution / 72.0, resolution / 72.0);

    KisPaintLayer *layer = new KisPaintLayer(image.data(), PkString("Page 1"), quint8_MAX);
    layer->paintDevice()->convertFromQImage(rendered, nullptr, 0, 0);
    image->addNode(layer, image->rootLayer(), 0);
    setProgress(100.0);

    document->setCurrentImage(image);
    return ImportExportCodes::OK;
}
