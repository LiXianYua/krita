/*
 *  SPDX-FileCopyrightText: 2014 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2017 Victor Wåhlström <victor.wahlstrom@initiali.se>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "kis_heightmap_import.h"

#include "../kis_impex_static_registration.h"
#include <PkDataStream.h>
#include <ctype.h>
#include <cmath>
#include <KisImportExportManager.h>
#include <KoColorSpaceRegistry.h>
#include <KoColorModelStandardIds.h>
#include <KoColorSpace.h>
#include <KoColorSpaceTraits.h>

#include <kis_debug.h>
#include <KisDocument.h>
#include <kis_group_layer.h>
#include <kis_image.h>
#include <kis_paint_layer.h>
#include <kis_paint_device.h>
#include <kis_iterator_ng.h>
#include <kis_random_accessor_ng.h>

#include "kis_heightmap_utils.h"

extern "C" KRITAIMPEX_EXPORT bool registerKisHeightMapImportFilter()
{
    static bool registered = false;
    return registerKisImpexFilterOnce(
        registered, {PkString("image/x-r32"), PkString("image/x-r16"), PkString("image/x-r8")}, {}, 1,
        []() -> KisImportExportFilter * { return new KisHeightMapImport(nullptr, PkVariantList()); });
}

template<typename T>
void fillData(KisPaintDeviceSP pd, int w, int h, PkDataStream &stream) {
    KIS_ASSERT_RECOVER_RETURN(pd);

    T pixel;

    for (int i = 0; i < h; ++i) {
        KisHLineIteratorSP it = pd->createHLineIteratorNG(0, i, w);
        do {
            stream >> pixel;
            KoGrayTraits<T>::setGray(it->rawData(), pixel);
            KoGrayTraits<T>::setOpacity(it->rawData(), OPACITY_OPAQUE_F, 1);
        } while(it->nextPixel());
    }
}

KisHeightMapImport::KisHeightMapImport(PkObject *parent, const PkVariantList &) : KisImportExportFilter(parent)
{
}

KisHeightMapImport::~KisHeightMapImport()
{
}

KisImportExportErrorCode KisHeightMapImport::convert(KisDocument *document, PkStream *io, KisPropertiesConfigurationSP configuration)
{
    (void)configuration;
    KoID depthId = KisHeightmapUtils::mimeTypeToKoID(mimeType());
    if (depthId.id().isEmpty()) {
        document->setErrorMessage(PkString("Unknown file type"));
        return ImportExportCodes::FileFormatIncorrect;
    }

    int w = 0;
    int h = 0;

    if (!io || !io->isOpen() || !io->isReadable()) {
        return ImportExportCodes::NoAccessToRead;
    }
    const PkStream::pk_int64 streamSize = io->size();
    if (streamSize <= 0) {
        return ImportExportCodes::FileFormatIncorrect;
    }
    const quint64 size = static_cast<quint64>(streamSize);

    PkDataStream::ByteOrder bo = PkDataStream::LittleEndian;

    const int pixelSize =
        depthId == Float32BitsColorDepthID ? 4 :
        depthId == Integer16BitsColorDepthID ? 2 : 1;

    const int configuredWidth = configuration ? configuration->getInt("width", 0) : 0;
    const int configuredHeight = configuration ? configuration->getInt("height", 0) : 0;
    if (!KisHeightmapUtils::resolveDimensions(size, pixelSize,
                                               configuredWidth, configuredHeight,
                                               w, h)) {
        return ImportExportCodes::FileFormatIncorrect;
    }
    bo = configuration && configuration->getInt("endianness", 1) == 0
        ? PkDataStream::BigEndian
        : PkDataStream::LittleEndian;

    PkDataStream s(io);
    s.setByteOrder(bo);
    // needed for 32bit float data
    s.setFloatingPointPrecision(PkDataStream::SinglePrecision);

    const KoColorSpace *colorSpace = KoColorSpaceRegistry::instance()->colorSpace(GrayAColorModelID.id(), depthId.id(), "Gray-D50-elle-V2-srgbtrc.icc");
    KisImageSP image = new KisImage(document->createUndoStore(), w, h, colorSpace, "imported heightmap");
    KisPaintLayerSP layer = new KisPaintLayer(image, image->nextLayerName(), 255);

    if (depthId == Float32BitsColorDepthID) {
        fillData<float>(layer->paintDevice(), w, h, s);
    }
    else if (depthId == Integer16BitsColorDepthID) {
        fillData<quint16>(layer->paintDevice(), w, h, s);
    }
    else if (depthId == Integer8BitsColorDepthID) {
        fillData<quint8>(layer->paintDevice(), w, h, s);
    }
    else {
        KIS_ASSERT_RECOVER_RETURN_VALUE(true, ImportExportCodes::InternalError);
        return ImportExportCodes::InternalError;
    }

    if (s.status() != PkDataStream::Ok) {
        return ImportExportCodes::ErrorWhileReading;
    }

    image->addNode(layer.data(), image->rootLayer().data());
    document->setCurrentImage(image);
    return ImportExportCodes::OK;
}
