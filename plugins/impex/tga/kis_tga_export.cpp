/*
 *  SPDX-FileCopyrightText: 2007 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_tga_export.h"

#include <kpluginfactory.h>
#include <PkDataStream.h>
#include <PkImage.h>
#include <PkRgb.h>
#include <KoColorModelStandardIds.h>
#include <KisExportCheckRegistry.h>
#include <KisImportExportBackend.h>
#include <KisImportExportManager.h>
#include <kis_paint_device.h>
#include <kis_image.h>
#include <kis_paint_layer.h>

#include "tga.h"

K_PLUGIN_FACTORY_WITH_JSON(KisTGAExportFactory, "krita_tga_export.json", registerPlugin<KisTGAExport>();)

KisTGAExport::KisTGAExport(QObject *parent, const PkVariantList &) : KisImportExportFilter(parent)
{
}

KisTGAExport::~KisTGAExport()
{
}

KisImportExportErrorCode KisTGAExport::convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration)
{
    (void)configuration;
    KisImageSP savingImage = kisImportExportSavingImage(document);
    PkRect rc = savingImage->bounds();
    PkImage image = savingImage->projection()->convertToQImage(0, 0, 0, rc.width(), rc.height(), KoColorConversionTransformation::internalRenderingIntent(), KoColorConversionTransformation::internalConversionFlags());

    PkDataStream s(io);
    s.setByteOrder(PkDataStream::LittleEndian);

    const PkImage& img = image;
    const bool hasAlpha = (img.format() == PkImage::Format_ARGB32);
    static constexpr quint8 originTopLeft = TGA_ORIGIN_UPPER + TGA_ORIGIN_LEFT; // 0x20
    static constexpr quint8 alphaChannel8Bits = 0x08;

    for (int i = 0; i < 12; i++)
        s << targaMagic[i];

    // write header
    s << quint16(img.width());   // width
    s << quint16(img.height());   // height
    s << quint8(hasAlpha ? 32 : 24);   // depth (24 bit RGB + 8 bit alpha)
    s << quint8(hasAlpha ? originTopLeft + alphaChannel8Bits : originTopLeft);   // top left image (0x20) + 8 bit alpha (0x8)

    for (int y = 0; y < img.height(); y++) {
        for (int x = 0; x < img.width(); x++) {
            const PkRgb color = img.pixel(x, y);
            s << quint8(pkBlue(color));
            s << quint8(pkGreen(color));
            s << quint8(pkRed(color));
            if (hasAlpha)
                s << quint8(pkAlpha(color));
        }
    }

    return s.status() == PkDataStream::Ok
        ? KisImportExportErrorCode(ImportExportCodes::OK)
        : KisImportExportErrorCode(ImportExportCodes::ErrorWhileWriting);
}

void KisTGAExport::initializeCapabilities()
{

    PkList<std::pair<KoID, KoID> > supportedColorModels;
    supportedColorModels << std::pair<KoID, KoID>()
            << std::pair<KoID, KoID>(RGBAColorModelID, Integer8BitsColorDepthID);
    addSupportedColorModels(supportedColorModels, "TGA");
}



#include "kis_tga_export.moc"
