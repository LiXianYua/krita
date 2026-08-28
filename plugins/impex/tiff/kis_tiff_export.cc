/*
 *  SPDX-FileCopyrightText: 2005 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2022 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_tiff_export.h"

#include "../kis_impex_static_registration.h"
#include <PkMemoryStream.h>

#include <memory>
#include <limits>

#include <exiv2/exiv2.hpp>
#include <tiffio.h>

#include <KisDocument.h>
#include <KisExportCheckRegistry.h>
#include <KoColorModelStandardIds.h>
#include <KoDocumentInfo.h>
#include <KoUnit.h>
#include <kis_assert.h>
#include <kis_group_layer.h>
#include <kis_layer_utils.h>
#include <kis_meta_data_backend_registry.h>
#include <kis_paint_layer.h>
#include <kis_tiff_writer_visitor.h>
#include <KisExiv2IODevice.h>

#include <config-tiff.h>
#ifdef TIFF_CAN_WRITE_PSD_TAGS
#include "kis_tiff_psd_writer_visitor.h"
#endif

#include "kis_tiff_converter.h"
#include "kis_tiff_logger.h"
#include "tiff_stream_adapter.h"

extern "C" KRITAIMPEX_EXPORT bool registerKisTIFFExportFilter()
{
    static bool registered = false;
    return registerKisImpexFilterOnce(
        registered, {}, {PkString("image/tiff")}, 1,
        []() -> KisImportExportFilter * { return new KisTIFFExport(nullptr, PkVariantList()); });
}

KisTIFFExport::KisTIFFExport(PkObject *parent, const PkVariantList &)
    : KisImportExportFilter(parent)
    , oldErrHandler(TIFFSetErrorHandler(&KisTiffErrorHandler))
    , oldWarnHandler(TIFFSetWarningHandler(&KisTiffWarningHandler))
{
}

KisTIFFExport::~KisTIFFExport()
{
    TIFFSetErrorHandler(oldErrHandler);
    TIFFSetWarningHandler(oldWarnHandler);
}

KisImportExportErrorCode KisTIFFExport::convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration)
{
    if (!io || !io->isWritable()) {
        return ImportExportCodes::NoAccessToWrite;
    }

    // If a configuration object was passed to the convert method, we use that, otherwise we load from the settings
    KisPropertiesConfigurationSP cfg(new KisPropertiesConfiguration());
    if (configuration) {
        cfg->fromXML(configuration->toXML());
    }
    else {
        cfg = lastSavedConfiguration(KisDocument::nativeFormatMimeType(), "image/tiff");
    }

    KisTIFFOptions options;
    options.fromProperties(configuration);

    if (!options.flatten && !options.saveAsPhotoshop) {
        const bool hasGroupLayers =
            KisLayerUtils::recursiveFindNode(document->savingImage()->root(),
                [] (KisNodeSP node) {
                    return node->parent() && node->inherits("KisGroupLayer");
                });
        options.flatten = hasGroupLayers;
    }

    KisImageSP kisimage = [&]() {
        if (options.flatten) {
            KisImageSP image =
                new KisImage(nullptr,
                             document->savingImage()->width(),
                             document->savingImage()->height(),
                             document->savingImage()->colorSpace(),
                             "");
            image->setResolution(document->savingImage()->xRes(),
                                 document->savingImage()->yRes());
            KisPaintDeviceSP pd = KisPaintDeviceSP(
                new KisPaintDevice(*document->savingImage()->projection()));
            KisPaintLayerSP l =
                KisPaintLayerSP(new KisPaintLayer(image.data(),
                                                  "projection",
                                                  OPACITY_OPAQUE_U8,
                                                  pd));
            image->addNode(KisNodeSP(l.data()), image->rootLayer().data());
            return image;
        } else {
            return document->savingImage();
        }
    }();

    dbgFile << "Start writing TIFF File";
    KIS_ASSERT_RECOVER_RETURN_VALUE(kisimage, ImportExportCodes::InternalError);

    PkMemoryStream encodedTiff;
    if (!encodedTiff.open(PkStream::ReadWrite)) {
        return ImportExportCodes::ErrorWhileWriting;
    }
    std::unique_ptr<TIFF, decltype(&TIFFCleanup)> image(
        kisTiffOpenStream(&encodedTiff, "w"), &TIFFCleanup);

    if (!image) {
        dbgFile << "Could not open the file for writing" << filename();
        return ImportExportCodes::NoAccessToWrite;
    }

    // Set the document information
    KoDocumentInfo *info = document->documentInfo();
    PkString title = info->aboutInfo("title");
    if (!title.isEmpty()) {
        if (!TIFFSetField(image.get(),
                          TIFFTAG_DOCUMENTNAME,
                          title.toLatin1().constData())) {
            return ImportExportCodes::ErrorWhileWriting;
        }
    }
    PkString abstract = info->aboutInfo("description");
    if (!abstract.isEmpty()) {
        if (!TIFFSetField(image.get(),
                          TIFFTAG_IMAGEDESCRIPTION,
                          abstract.toLatin1().constData())) {
            return ImportExportCodes::ErrorWhileWriting;
        }
    }
    PkString author = info->authorInfo("creator");
    if (!author.isEmpty()) {
        if (!TIFFSetField(image.get(),
                          TIFFTAG_ARTIST,
                          author.toLatin1().constData())) {
            return ImportExportCodes::ErrorWhileWriting;
        }
    }

    dbgFile << "xres: " << INCH_TO_POINT(kisimage->xRes())
            << " yres: " << INCH_TO_POINT(kisimage->yRes());
    if (!TIFFSetField(
            image.get(),
            TIFFTAG_XRESOLUTION,
            INCH_TO_POINT(kisimage->xRes()))) { // It is the "invert" macro
                                                // because we convert from
                                                // pointer-per-inch to points
        return ImportExportCodes::ErrorWhileWriting;
    }
    if (!TIFFSetField(image.get(),
                      TIFFTAG_YRESOLUTION,
                      INCH_TO_POINT(kisimage->yRes()))) {
        return ImportExportCodes::ErrorWhileWriting;
    }

    if (!TIFFSetField(image.get(), TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH)) {
        return ImportExportCodes::ErrorWhileWriting;
    }

    KisGroupLayer *root =
        dynamic_cast<KisGroupLayer *>(kisimage->rootLayer().data());
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(root,
                                         ImportExportCodes::InternalError);

#ifdef TIFF_CAN_WRITE_PSD_TAGS
    if (options.saveAsPhotoshop) {
        KisTiffPsdWriter writer(image.get(), &options);
        KisImportExportErrorCode result = writer.writeImage(root);
        if (!result.isOk()) {
            return result;
        }
    } else
#endif // TIFF_CAN_WRITE_PSD_TAGS
    {
        KisTIFFWriterVisitor visitor(image.get(), &options);
        if (!(visitor.visit(root))) {
            return ImportExportCodes::Failure;
        }
    }

    image.reset();

    if (!options.flatten && !options.saveAsPhotoshop) {
        // HACK!! Externally inject the Exif metadata
        // libtiff has no way to access the fields wholesale
        try {
            if (encodedTiff.size() <= 0 ||
                static_cast<std::uint64_t>(encodedTiff.size()) >
                    std::numeric_limits<std::size_t>::max()) {
                return ImportExportCodes::ErrorWhileWriting;
            }
            KisExiv2IODevice::ptr_type basicIoDevice(new Exiv2::MemIo(
                reinterpret_cast<const Exiv2::byte *>(encodedTiff.data()),
                static_cast<std::size_t>(encodedTiff.size())));

#if EXIV2_TEST_VERSION(0,28,0)
            const std::unique_ptr<Exiv2::Image> img = Exiv2::ImageFactory::open(std::move(basicIoDevice));
#else
            const std::unique_ptr<Exiv2::Image> img(Exiv2::ImageFactory::open(basicIoDevice).release());
#endif

            img->readMetadata();

            Exiv2::ExifData &data = img->exifData();

            const KisMetaData::IOBackend *backend =
                KisMetadataBackendRegistry::instance()->value("exif");

            // All IFDs are paint layer children of root
            KisNodeSP node = root->firstChild();

            PkMemoryStream ioDevice;

            // Get layer
            KisLayer *layer = dynamic_cast<KisLayer *>(node.data());
            Q_ASSERT(layer);

            // Inject the data as any other IOBackend
            if (!backend || !ioDevice.open(PkStream::WriteOnly) ||
                !backend->saveTo(layer->metaData(), &ioDevice) ||
                ioDevice.size() > std::numeric_limits<uint32_t>::max()) {
                return ImportExportCodes::ErrorWhileWriting;
            }

            Exiv2::ExifData dataToInject;

            // Reinterpret the blob we just got and inject its contents into
            // tempData
            Exiv2::ExifParser::decode(
                dataToInject,
                reinterpret_cast<const Exiv2::byte *>(ioDevice.data()),
                static_cast<uint32_t>(ioDevice.size()));

            for (const auto &v : dataToInject) {
                data[v.key()] = v.value();
            }
            // Write metadata
            img->writeMetadata();
            Exiv2::BasicIo &modified = img->io();
            const std::size_t modifiedSize = modified.size();
            const Exiv2::byte *modifiedData = modified.mmap();
            if (!modifiedData || !kisTiffWriteExact(
                    *io,
                    reinterpret_cast<const char *>(modifiedData),
                    modifiedSize)) {
                return ImportExportCodes::ErrorWhileWriting;
            }
            return ImportExportCodes::OK;
#if EXIV2_TEST_VERSION(0,28,0)
        } catch (Exiv2::Error &e) {
            errFile << "Failed injecting TIFF metadata:" << Exiv2::Error(e.code()).what();
#else
        } catch (Exiv2::AnyError &e) {
            errFile << "Failed injecting TIFF metadata:" << e.code()
                    << e.what();
#endif
            return ImportExportCodes::ErrorWhileWriting;
        }
    }
    return kisTiffWriteExact(*io,
                             encodedTiff.data(),
                             static_cast<std::size_t>(encodedTiff.size()))
        ? ImportExportCodes::OK
        : ImportExportCodes::ErrorWhileWriting;
}

KisPropertiesConfigurationSP KisTIFFExport::defaultConfiguration(const PkByteArray &/*from*/, const PkByteArray &/*to*/) const
{
    KisTIFFOptions options;
    return options.toProperties();
}

void KisTIFFExport::initializeCapabilities()
{
    addCapability(KisExportCheckRegistry::instance()->get("MultiLayerCheck")->create(KisExportCheckBase::SUPPORTED));
    addCapability(KisExportCheckRegistry::instance()->get("LayerOpacityCheck")->create(KisExportCheckBase::PARTIALLY));
    addCapability(KisExportCheckRegistry::instance()->get("sRGBProfileCheck")->create(KisExportCheckBase::SUPPORTED));
    addCapability(KisExportCheckRegistry::instance()->get("ExifCheck")->create(KisExportCheckBase::SUPPORTED));
    addCapability(KisExportCheckRegistry::instance()
                      ->get("TiffExifCheck")
                      ->create(KisExportCheckBase::PARTIALLY));
    addCapability(
        KisExportCheckRegistry::instance()->get("ColorModelHomogenousCheck")->create(KisExportCheckBase::SUPPORTED));

    PkList<std::pair<KoID, KoID>> supportedColorModels = {
        {},
        {RGBAColorModelID, Integer8BitsColorDepthID},
        {RGBAColorModelID, Integer16BitsColorDepthID},
        {RGBAColorModelID, Float16BitsColorDepthID},
        {RGBAColorModelID, Float32BitsColorDepthID},
        {GrayAColorModelID, Integer8BitsColorDepthID},
        {GrayAColorModelID, Integer16BitsColorDepthID},
        {CMYKAColorModelID, Integer8BitsColorDepthID},
        {CMYKAColorModelID, Integer16BitsColorDepthID},
        {YCbCrAColorModelID, Integer8BitsColorDepthID},
        {YCbCrAColorModelID, Integer16BitsColorDepthID},
        {YCbCrAColorModelID, Float16BitsColorDepthID},
        {YCbCrAColorModelID, Float32BitsColorDepthID},
        {LABAColorModelID, Integer8BitsColorDepthID},
        {LABAColorModelID, Integer16BitsColorDepthID},
        {LABAColorModelID, Float16BitsColorDepthID},
        {LABAColorModelID, Float32BitsColorDepthID}};
    addSupportedColorModels(supportedColorModels, "TIFF");

}
