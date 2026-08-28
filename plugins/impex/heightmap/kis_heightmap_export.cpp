/*
 *  SPDX-FileCopyrightText: 2014 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2017 Victor Wåhlström <victor.wahlstrom@initiali.se>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "kis_heightmap_export.h"

#include "../kis_impex_static_registration.h"
#include <PkDataStream.h>
#include <KoColorSpace.h>
#include <KoColorSpaceConstants.h>
#include <KoColorSpaceTraits.h>
#include <KoColorSpaceRegistry.h>
#include <KoColorModelStandardIds.h>

#include <KisImportExportManager.h>
#include <KisImportExportBackend.h>
#include <KisExportCheckRegistry.h>

#include <kis_image.h>
#include <kis_paint_device.h>
#include <kis_properties_configuration.h>
#include <kis_iterator_ng.h>
#include <kis_random_accessor_ng.h>

#include "kis_heightmap_utils.h"

extern "C" KRITAIMPEX_EXPORT bool registerKisHeightMapExportFilter()
{
    static bool registered = false;
    return registerKisImpexFilterOnce(
        registered, {}, {PkString("image/x-r32"), PkString("image/x-r16"), PkString("image/x-r8")}, 1,
        []() -> KisImportExportFilter * { return new KisHeightMapExport(nullptr, PkVariantList()); });
}

template<typename T>
static void writeData(KisPaintDeviceSP pd, const PkRect &bounds, PkDataStream &out_stream)
{
    KIS_ASSERT_RECOVER_RETURN(pd);

    KisSequentialConstIterator it(pd, bounds);
    while (it.nextPixel()) {
        out_stream << KoGrayTraits<T>::gray(const_cast<quint8*>(it.rawDataConst()));
    }
}

KisHeightMapExport::KisHeightMapExport(PkObject *parent, const PkVariantList &) : KisImportExportFilter(parent)
{
}

KisHeightMapExport::~KisHeightMapExport()
{
}

KisPropertiesConfigurationSP KisHeightMapExport::defaultConfiguration(const PkByteArray &from, const PkByteArray &to) const
{
    (void)from;
    (void)to;
    KisPropertiesConfigurationSP cfg = new KisPropertiesConfiguration();
    cfg->setProperty("endianness", 0);
    return cfg;
}

void KisHeightMapExport::initializeCapabilities()
{
    if (mimeType() == PkByteArray("image/x-r8", sizeof("image/x-r8") - 1)) {
        PkList<std::pair<KoID, KoID> > supportedColorModels;
        supportedColorModels << std::pair<KoID, KoID>()
                << std::pair<KoID, KoID>(GrayAColorModelID, Integer8BitsColorDepthID);
        addSupportedColorModels(supportedColorModels, "R8 Heightmap");
    }
    else if (mimeType() == PkByteArray("image/x-r16", sizeof("image/x-r16") - 1)) {
        PkList<std::pair<KoID, KoID> > supportedColorModels;
        supportedColorModels << std::pair<KoID, KoID>()
                << std::pair<KoID, KoID>(GrayAColorModelID, Integer16BitsColorDepthID);
        addSupportedColorModels(supportedColorModels, "R16 Heightmap");
    }
    else if (mimeType() == PkByteArray("image/x-r32", sizeof("image/x-r32") - 1)) {
        PkList<std::pair<KoID, KoID> > supportedColorModels;
        supportedColorModels << std::pair<KoID, KoID>()
                << std::pair<KoID, KoID>(GrayAColorModelID, Float32BitsColorDepthID);
        addSupportedColorModels(supportedColorModels, "R32 Heightmap");
    }
}

KisImportExportErrorCode KisHeightMapExport::convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration)
{
    const PkByteArray mime = mimeType();
    KIS_ASSERT_RECOVER_RETURN_VALUE(
        mime == PkByteArray("image/x-r16", sizeof("image/x-r16") - 1) ||
        mime == PkByteArray("image/x-r8", sizeof("image/x-r8") - 1) ||
        mime == PkByteArray("image/x-r32", sizeof("image/x-r32") - 1),
        ImportExportCodes::FileFormatIncorrect);

    KisImageSP image = kisImportExportSavingImage(document);
    PkDataStream::ByteOrder bo = configuration->getInt("endianness", 1) == 0 ? PkDataStream::BigEndian : PkDataStream::LittleEndian;

    KisPaintDeviceSP pd = new KisPaintDevice(*image->projection());

    PkDataStream s(io);
    s.setByteOrder(bo);
    // needed for 32bit float data
    s.setFloatingPointPrecision(PkDataStream::SinglePrecision);

    KoID target_co_model = GrayAColorModelID;
    KoID target_co_depth = KisHeightmapUtils::mimeTypeToKoID(mimeType());
    KIS_ASSERT(!target_co_depth.id().isEmpty());

    if (pd->colorSpace()->colorModelId() != target_co_model || pd->colorSpace()->colorDepthId() != target_co_depth) {
        pd = new KisPaintDevice(*pd.data());
        pd->convertTo(KoColorSpaceRegistry::instance()->colorSpace(target_co_model.id(), target_co_depth.id()));
    }

    if (target_co_depth == Float32BitsColorDepthID) {
        writeData<float>(pd, image->bounds(), s);
    }
    else if (target_co_depth == Integer16BitsColorDepthID) {
        writeData<quint16>(pd, image->bounds(), s);
    }
    else if (target_co_depth == Integer8BitsColorDepthID) {
        writeData<quint8>(pd, image->bounds(), s);
    }
    else {
        KIS_ASSERT_RECOVER_RETURN_VALUE(true, ImportExportCodes::InternalError);
        return ImportExportCodes::InternalError;
    }
    return s.status() == PkDataStream::Ok
        ? KisImportExportErrorCode(ImportExportCodes::OK)
        : KisImportExportErrorCode(ImportExportCodes::ErrorWhileWriting);
}
