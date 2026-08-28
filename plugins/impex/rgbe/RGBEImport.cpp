/*
 * SPDX-FileCopyrightText: 2023 Rasyuqa A. H. <qampidh@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Based on KImageFormats Radiance HDR loader
 *
 * SPDX-FileCopyrightText: 2005 Christoph Hormann <chris_hormann@gmx.de>
 * SPDX-FileCopyrightText: 2005 Ignacio Castaño <castanyo@yahoo.es>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include <PkMemoryStream.h>
#include "../kis_impex_static_registration.h"
#include <PkAuxTypes.h>

#include <KisDocument.h>
#include <KisImportExportErrorCode.h>
#include <PkDataStream.h>
#include <KoColorModelStandardIds.h>
#include <KoColorProfile.h>
#include <KoCompositeOpRegistry.h>
#include <kis_group_layer.h>
#include <kis_iterator_ng.h>
#include <kis_meta_data_backend_registry.h>
#include <kis_paint_layer.h>
#include <kis_painter.h>
#include <kis_properties_configuration.h>
#include <kis_sequential_iterator.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

#include "RGBEImport.h"
#include "RGBEImportUtils.h"
#include "rgbe_codec.h"

extern "C" KRITAIMPEX_EXPORT bool registerRGBEImportFilter()
{
    static bool registered = false;
    return registerKisImpexFilterOnce(
        registered, {PkString("image/vnd.radiance")}, {}, 1,
        []() -> KisImportExportFilter * { return new RGBEImport(nullptr, PkVariantList()); });
}

#define MAXLINE 1024

class Q_DECL_HIDDEN RGBEImportData
{
public:
    KisPaintDeviceSP m_currentFrame{nullptr};
    KoID m_colorID;
    KoID m_depthID;
    float m_gamma = 1.0;
    float m_exposure = 1.0;
    const KoColorSpace *cs = nullptr;
};

RGBEImport::RGBEImport(PkObject *parent, const PkVariantList &)
    : KisImportExportFilter(parent)
{
}

KisImportExportErrorCode
RGBEImport::convert(KisDocument *document, PkStream *io, KisPropertiesConfigurationSP /*configuration*/)
{
    if (!io->isReadable()) {
        errFile << "Cannot read image contents";
        return ImportExportCodes::NoAccessToRead;
    }

    char signature[11] = {};
    const auto signatureSize = io->peek(signature, sizeof(signature));
    if (!((signatureSize >= 11 && std::memcmp(signature, "#?RADIANCE\n", 11) == 0) ||
          (signatureSize >= 7 && std::memcmp(signature, "#?RGBE\n", 7) == 0))) {
        errFile << "Invalid RGBE header!";
        return ImportExportCodes::ErrorWhileReading;
    }

    RGBEImportData d{};

    int len;
    PkByteArray line;
    line.resize(MAXLINE + 1);
    std::string rawFormat;
    std::string rawGamma;
    std::string rawExposure;
    std::string rawHeaderInfo;

    // Parse header
    do {
        len = io->readLine(line.data(), MAXLINE);
        const std::string_view current(line.constData(), len > 0 ? static_cast<std::size_t>(len) : 0);
        const auto field = [&current](std::string_view prefix) {
            if (current.substr(0, prefix.size()) != prefix) {
                return std::string();
            }
            std::string value(current.substr(prefix.size()));
            while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
                value.pop_back();
            }
            return value;
        };
        if (current.substr(0, 2) == "# ") {
            rawHeaderInfo = field("# ");
        } else if (current.substr(0, 6) == "GAMMA=") {
            rawGamma = field("GAMMA=");
        } else if (current.substr(0, 9) == "EXPOSURE=") {
            rawExposure = field("EXPOSURE=");
        } else if (current.substr(0, 7) == "FORMAT=") {
            rawFormat = field("FORMAT=");
        }
    } while ((len > 0) && (line.constData()[0] != '\n'));

    if (rawFormat != "32-bit_rle_rgbe") {
        errFile << "Invalid RGBE format!";
        return ImportExportCodes::ErrorWhileReading;
    }

    const PkString headerInfo = [&]() {
        if (!rawHeaderInfo.empty()) {
            return PkString(rawHeaderInfo.c_str()).trimmed();
        }
        return PkString();
    }();

    // Unused fields, I don't know what to do with gamma and exposure fields yet.
    if (!rawGamma.empty()) {
        char *end = nullptr;
        const float gammaTemp = std::strtof(rawGamma.c_str(), &end);
        if (end == rawGamma.c_str() + rawGamma.size()) {
            d.m_gamma = gammaTemp;
        }
    }
    if (!rawExposure.empty()) {
        char *end = nullptr;
        const float expTemp = std::strtof(rawExposure.c_str(), &end);
        if (end == rawExposure.c_str() + rawExposure.size()) {
            d.m_exposure = expTemp;
        }
    }

    len = io->readLine(line.data(), MAXLINE);
    if (len <= 0) {
        return ImportExportCodes::FileFormatIncorrect;
    }
    line.resize(len);

    /*
       TODO: handle flipping and rotation, as per the spec below
       The single resolution line consists of 4 values, a X and Y label each followed by a numerical
       integer value. The X and Y are immediately preceded by a sign which can be used to indicate
       flipping, the order of the X and Y indicate rotation. The standard coordinate system for
       Radiance images would have the following resolution string -Y N +X N. This indicates that the
       vertical axis runs down the file and the X axis is to the right (imagining the image as a
       rectangular block of data). A -X would indicate a horizontal flip of the image. A +Y would
       indicate a vertical flip. If the X value appears before the Y value then that indicates that
       the image is stored in column order rather than row order, that is, it is rotated by 90 degrees.
       The reader can convince themselves that the 8 combinations cover all the possible image orientations
       and rotations.
    */
    RGBE::Resolution resolution;
    if (!RGBE::parseResolution(std::string_view(line.constData(), static_cast<std::size_t>(len)), resolution)) {
        errFile << "Invalid HDR file, the first line after the header didn't have the expected format:"
                << std::string(line.constData(), static_cast<std::size_t>(len));
        return ImportExportCodes::InternalError;
    }

    if (!resolution.rowMajor || resolution.yIncreasing || !resolution.xIncreasing) {
        errFile << "Unsupported image orientation in HDR file.";
        return ImportExportCodes::InternalError;
    }

    const int width = resolution.width;
    const int height = resolution.height;

    dbgFile << "RGBE image information:";
    dbgFile << "Program info:" << headerInfo;
    if (!rawGamma.empty()) {
        dbgFile << "Gamma:" << d.m_gamma;
    } else {
        dbgFile << "No gamma metadata provided";
    }
    if (!rawExposure.empty()) {
        dbgFile << "Exposure:" << d.m_exposure;
    } else {
        dbgFile << "No exposure metadata provided";
    }
    dbgFile << "Dimension:" << width << "x" << height;

    KisImageSP image;
    KisLayerSP layer;

    const KoColorProfile *profile = nullptr;

    d.m_colorID = RGBAColorModelID;
    d.m_depthID = Float32BitsColorDepthID;

    profile = KoColorSpaceRegistry::instance()->p709G10Profile();
    d.cs = KoColorSpaceRegistry::instance()->colorSpace(d.m_colorID.id(), d.m_depthID.id(), profile);

    image = new KisImage(document->createUndoStore(), width, height, d.cs, "RGBE image");
    layer = new KisPaintLayer(image, image->nextLayerName(), OPACITY_OPAQUE_U8);
    d.m_currentFrame = new KisPaintDevice(image->colorSpace());

    PkDataStream stream(io);
    KisSequentialIterator it(d.m_currentFrame, {0, 0, width, height});

    if (!RGBEIMPORT::LoadHDR(stream, io, width, height, it)) {
        errFile << "Error loading HDR file.";
        return ImportExportCodes::InternalError;
    }

    layer->paintDevice()->makeCloneFrom(d.m_currentFrame, image->bounds());
    image->addNode(layer, image->rootLayer().data());

    document->setCurrentImage(image);

    return ImportExportCodes::OK;
}
