/*
 *  SPDX-FileCopyrightText: 2013 Sven Langkamp <sven.langkamp@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "qml_converter.h"
#include "qml_format.h"

#include <filesystem>
#include <system_error>
#include <vector>

#include <kis_image.h>
#include <kis_group_layer.h>
#include <KisPngCodec.h>
#include <PkStringList.h>

KisImportExportErrorCode QMLConverter::buildFile(const PkString &filename, const PkString &realFilename, PkStream *io, KisImageSP image)
{
    if (!io || !image) {
        return ImportExportCodes::InternalError;
    }

    const PkString effectiveRealFilename = realFilename.isEmpty() ? filename : realFilename;
    const std::filesystem::path realPath = std::filesystem::u8path(effectiveRealFilename.PkToUtf8());
    const std::string imageDirectoryName = realPath.stem().u8string() + "_images";
    const std::filesystem::path imagePath = realPath.parent_path() / imageDirectoryName;

    KisNodeSP node = image->rootLayer()->firstChild();
    if (node) {
        std::error_code error;
        std::filesystem::create_directories(imagePath, error);
        if (error) {
            return ImportExportCodes::CannotCreateFile;
        }
    }

    std::vector<QmlLayerRecord> layers;
    while(node) {
        KisPaintDeviceSP projection = node->projection();
        PkRect rect = projection->exactBounds();
        const PkString name = pkStringReplaceAll(node->name().toLower(), " ", "_", PkCaseSensitive);
        const std::string fileName = name.PkToUtf8() + ".png";
        const std::filesystem::path filePath = imagePath / fileName;

        KisPNGOptions options;
        PkVector<KisAnnotationSP> annotations;
        KisPngCodec png;
        const KisImportExportErrorCode pngResult =
            png.buildFile(PkString(filePath.u8string().c_str()),
                          rect,
                          image->xRes(),
                          image->yRes(),
                          projection,
                          annotations.begin(),
                          annotations.end(),
                          options,
                          nullptr);
        if (!pngResult.isOk()) {
            return pngResult;
        }

        layers.push_back({name,
                          rect,
                          PkString((std::filesystem::path(imageDirectoryName) / fileName)
                                       .generic_string().c_str()),
                          node->opacity() / 255.0});
        node = node->nextSibling();
    }

    return writeQmlDocument(io, image->width(), image->height(), layers)
        ? KisImportExportErrorCode(ImportExportCodes::OK)
        : KisImportExportErrorCode(ImportExportCodes::ErrorWhileWriting);
}
