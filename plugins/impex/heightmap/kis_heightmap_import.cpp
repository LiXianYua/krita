/*
 *  SPDX-FileCopyrightText: 2014 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2017 Victor Wåhlström <victor.wahlstrom@initiali.se>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "kis_heightmap_import.h"

#include <ctype.h>
#include <cmath>

#include <kpluginfactory.h>

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
#include <kis_transaction.h>
#include <kis_iterator_ng.h>
#include <kis_random_accessor_ng.h>

#include "kis_heightmap_utils.h"

K_PLUGIN_FACTORY_WITH_JSON(HeightMapImportFactory, "krita_heightmap_import.json", registerPlugin<KisHeightMapImport>();)

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

KisHeightMapImport::KisHeightMapImport(QObject *parent, const PkVariantList &) : KisImportExportFilter(parent)
{
}

KisHeightMapImport::~KisHeightMapImport()
{
}

KisImportExportErrorCode KisHeightMapImport::convert(KisDocument *document, PkStream *io, KisPropertiesConfigurationSP configuration)
{
    (void)configuration;
    KoID depthId = KisHeightmapUtils::mimeTypeToKoID(mimeType());
    if (depthId.id().isNull()) {
        document->setErrorMessage(PkString("Unknown file type"));
        return ImportExportCodes::FileFormatIncorrect;
    }

    int w = 0;
    int h = 0;

    KIS_ASSERT(io->isOpen());
    const quint64 size = io->size();
    if (size == 0) {
        return ImportExportCodes::FileFormatIncorrect;
    }

    PkDataStream::ByteOrder bo = PkDataStream::LittleEndian;

    const int pixelSize =
        depthId == Float32BitsColorDepthID ? 4 :
        depthId == Integer16BitsColorDepthID ? 2 : 1;

    if (size % pixelSize != 0) {
        return ImportExportCodes::FileFormatIncorrect;
    }
    const int numPixels = size / pixelSize;

    w = std::sqrt(numPixels);
    if (w <= 0 || numPixels % w != 0) {
        return ImportExportCodes::FileFormatIncorrect;
    }
    h = numPixels / w;
    bo = PkDataStream::LittleEndian;

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

#include "kis_heightmap_import.moc"
