/*
 * SPDX-FileCopyrightText: 2023 Rasyuqa A. H. <qampidh@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "RGBEExport.h"

#include "../kis_impex_static_registration.h"
#include <KisGlobalResourcesInterface.h>
#include <PkMemoryStream.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <KisExportCheckRegistry.h>
#include <KisImportExportBackend.h>
#include <KisImportExportErrorCode.h>
#include <KoAlwaysInline.h>
#include <KoColorModelStandardIds.h>
#include <KoColorProfile.h>
#include <KoColorSpace.h>
#include <KoColorTransferFunctions.h>
#include <kis_assert.h>
#include <kis_debug.h>
#include <kis_iterator_ng.h>
#include <kis_layer.h>
#include <kis_layer_utils.h>
#include <kis_painter.h>
#include <kis_properties_configuration.h>
#include <kis_sequential_iterator.h>

extern "C" KRITAIMPEX_EXPORT void registerRGBEExportFilter()
{
    static bool registered = false;
    registerKisImpexFilterOnce(
        registered, {}, {PkString("image/vnd.radiance")}, 1,
        []() -> KisImportExportFilter * { return new RGBEExport(nullptr, PkVariantList()); });
}

namespace RGBE
{
inline std::vector<quint8> floatToRGBE(const int width, const int height, KisPaintDeviceSP &dev)
{
    KisSequentialConstIterator it(dev, {0, 0, width, height});
    std::vector<quint8> res(static_cast<std::size_t>(width) * height * 4);

    quint8 rgbe[4] = {0, 0, 0, 0};

    quint8 *ptr = reinterpret_cast<quint8 *>(res.data());
    while (it.nextPixel()) {
        auto *src = reinterpret_cast<const float *>(it.rawDataConst());
        auto *dst = reinterpret_cast<quint8 *>(ptr);
        float vMax = std::max(src[2], std::max(src[0], src[1]));
        int exp;

        if (vMax < 1e-32) {
            rgbe[0] = rgbe[1] = rgbe[2] = rgbe[3] = 0;
        } else {
            vMax = frexp(vMax, &exp) * 256.0f / vMax;
            // Clamp negative values
            rgbe[0] = static_cast<quint8>(std::max(src[0], 0.0f) * vMax);
            rgbe[1] = static_cast<quint8>(std::max(src[1], 0.0f) * vMax);
            rgbe[2] = static_cast<quint8>(std::max(src[2], 0.0f) * vMax);
            rgbe[3] = static_cast<quint8>(exp + 128);
        }

        std::memcpy(dst, rgbe, 4);

        ptr += 4;
    }

    return res;
}

inline void writeBytesRLE(std::vector<quint8> &rleBuffer, const quint8 *data, int nBytes)
{
    static constexpr int minRunLen = 4;
    int cur = 0;
    int begRun;
    int runCount;
    int oldRunCount;
    int nonRunCount;

    quint8 buf[2];

    while (cur < nBytes) {
        begRun = cur;

        runCount = oldRunCount = 0;
        while ((runCount < minRunLen) && (begRun < nBytes)) {
            begRun += runCount;
            oldRunCount = runCount;
            runCount = 1;
            while ((begRun + runCount < nBytes) && (runCount < 127) && (data[begRun] == data[begRun + runCount])) {
                runCount++;
            }
        }

        if ((oldRunCount > 1) && (oldRunCount == begRun - cur)) {
            buf[0] = 128 + oldRunCount;
            buf[1] = data[cur];
            rleBuffer.insert(rleBuffer.end(), std::begin(buf), std::end(buf));
            cur = begRun;
        }

        while (cur < begRun) {
            nonRunCount = begRun - cur;
            if (nonRunCount > 128) {
                nonRunCount = 128;
            }
            buf[0] = nonRunCount;
            rleBuffer.push_back(buf[0]);
            rleBuffer.insert(rleBuffer.end(), data + cur, data + cur + nonRunCount);
            cur += nonRunCount;
        }

        if (runCount >= minRunLen) {
            buf[0] = 128 + runCount;
            buf[1] = data[begRun];
            rleBuffer.insert(rleBuffer.end(), std::begin(buf), std::end(buf));
            cur += runCount;
        }
    }
}
}

RGBEExport::RGBEExport(PkObject *parent, const PkVariantList &)
    : KisImportExportFilter{parent}
{
}

KisImportExportErrorCode RGBEExport::convert(KisDocument *document, PkStream *io, KisPropertiesConfigurationSP cfg)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(io->isWritable(), ImportExportCodes::NoAccessToWrite);

    KisImageSP image = kisImportExportSavingImage(document);
    const PkRect bounds = image->bounds();

    const KoColorSpace *cs = image->colorSpace();
    const KoColorProfile *targetProfile = KoColorSpaceRegistry::instance()->p709G10Profile();
    const KoColorSpace *targetCs = KoColorSpaceRegistry::instance()->colorSpace(RGBAColorModelID.id(),
                                                                                Float32BitsColorDepthID.id(),
                                                                                targetProfile);

    if (image->root()->childCount() > 1) {
        KisLayerUtils::flattenImage(image, nullptr);
        image->waitForDone();
    }

    const bool isLinearSrgb = [&]() {
        const bool hasPrimaries = cs->profile()->hasColorants();
        const TransferCharacteristics gamma = cs->profile()->getTransferCharacteristics();
        if (hasPrimaries) {
            const ColorPrimaries primaries = cs->profile()->getColorPrimaries();
            if (gamma == TRC_LINEAR && primaries == PRIMARIES_ITU_R_BT_709_5) {
                return true;
            }
        }
        return false;
    }();

    // Color profile will be lost on RGBE export; so convert it to F32, linear sRGB
    if (cs->colorModelId() != RGBAColorModelID || cs->colorDepthId() != Float32BitsColorDepthID || !isLinearSrgb) {
        dbgFile << "Image is not in linear sRGB, converting...";
        image->convertImageColorSpace(targetCs,
                                      KoColorConversionTransformation::internalRenderingIntent(),
                                      KoColorConversionTransformation::internalConversionFlags());
        image->waitForDone();
    }

    // Fill transparent pixels with full opacity
    KoColor bgColor(PkColor(255, 255, 255), targetCs);
    bgColor.fromKoColor(cfg->getColor("transparencyFillcolor"));

    KisPaintDeviceSP dev = new KisPaintDevice(targetCs);
    KisPainter gc(dev);

    dev->fill(PkRect(0, 0, image->width(), image->height()), bgColor);
    gc.bitBlt(PkPoint(0, 0), image->projection(), PkRect(0, 0, image->width(), image->height()));
    gc.end();

    // Get pixel data and convert it to RGBE format
    const std::vector<quint8> pixels = RGBE::floatToRGBE(bounds.width(), bounds.height(), dev);

    std::vector<quint8> fileBuffer;
    {
        // Write header
        const std::string header = "#?RADIANCE\n# Created with Krita RGBE Export\n"
                                   "FORMAT=32-bit_rle_rgbe\n\n-Y " +
            std::to_string(image->height()) + " +X " + std::to_string(image->width()) + "\n";
        fileBuffer.insert(fileBuffer.end(), header.begin(), header.end());
    }

    {
        // Write pixel data
        const int scanWidth = image->width();
        const int scanHeight = image->height();

        if ((scanWidth < 8) || (scanWidth > 0x7fff)) {
            // Invalid width, save without RLE
            fileBuffer.insert(fileBuffer.end(), pixels.begin(), pixels.end());
        } else {
            // Save with RLE
            std::vector<quint8> rleBuffer;
            std::vector<quint8> outputBuffer;

            int numScanline = scanHeight;
            quint8 rgbe[4];

            rleBuffer.resize(sizeof(quint8) * 4 * scanWidth);
            auto *src = reinterpret_cast<const quint8 *>(pixels.data());
            auto *rle = reinterpret_cast<quint8 *>(rleBuffer.data());

            while (numScanline-- > 0) {
                rgbe[0] = 2;
                rgbe[1] = 2;
                rgbe[2] = scanWidth >> 8;
                rgbe[3] = scanWidth & 0xFF;
                outputBuffer.insert(outputBuffer.end(), std::begin(rgbe), std::end(rgbe));

                for (int i = 0; i < scanWidth; i++) {
                    rle[i] = src[0];
                    rle[i + scanWidth] = src[1];
                    rle[i + 2 * scanWidth] = src[2];
                    rle[i + 3 * scanWidth] = src[3];
                    src += 4;
                }

                for (int i = 0; i < 4; i++) {
                    RGBE::writeBytesRLE(outputBuffer, &rle[i * scanWidth], scanWidth);
                    fileBuffer.insert(fileBuffer.end(), outputBuffer.begin(), outputBuffer.end());
                    outputBuffer.clear();
                }
            }
        }
    }

    if (io->write(reinterpret_cast<const char *>(fileBuffer.data()),
                  static_cast<PkStream::pk_int64>(fileBuffer.size())) !=
        static_cast<PkStream::pk_int64>(fileBuffer.size())) {
        return ImportExportCodes::ErrorWhileWriting;
    }

    return ImportExportCodes::OK;
}

void RGBEExport::initializeCapabilities()
{
    PkList<std::pair<KoID, KoID>> supportedColorModels;
    addCapability(KisExportCheckRegistry::instance()->get("AnimationCheck")->create(KisExportCheckBase::PARTIALLY));
    addCapability(KisExportCheckRegistry::instance()->get("sRGBProfileCheck")->create(KisExportCheckBase::PARTIALLY));
    addCapability(KisExportCheckRegistry::instance()->get("ExifCheck")->create(KisExportCheckBase::PARTIALLY));
    addCapability(KisExportCheckRegistry::instance()->get("MultiLayerCheck")->create(KisExportCheckBase::PARTIALLY));
    addCapability(KisExportCheckRegistry::instance()->get("TiffExifCheck")->create(KisExportCheckBase::PARTIALLY));
    supportedColorModels << std::pair<KoID, KoID>() << std::pair<KoID, KoID>(RGBAColorModelID, Float32BitsColorDepthID);
    addSupportedColorModels(supportedColorModels, "RGBE");
}

KisPropertiesConfigurationSP RGBEExport::defaultConfiguration(const PkByteArray &, const PkByteArray &) const
{
    KisPropertiesConfigurationSP cfg = new KisPropertiesConfiguration();

    KoColor background(KoColorSpaceRegistry::instance()->rgb8());
    background.fromQColor(PkColor(255, 255, 255));
    PkVariant v;
    v.setValue(background);

    cfg->setProperty("transparencyFillcolor", v);

    return cfg;
}
