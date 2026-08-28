/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisPkImageFileDecoderAdapter.h"

#include "KisDocument.h"
#include "KisDocumentRegistry.h"

#include <PkAuxTypes.h>
#include <PkImageFileDecoder.h>
#include <PkScopedPointer.h>
#include <PkString.h>
#include <kis_image.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{

const std::vector<std::string> &exoticExtensions()
{
    static const std::vector<std::string> extensions {
        "ani", "eps", "hdr", "icns", "pcx", "pic", "psd",
        "ras", "rgb", "rgba", "sgi", "svg", "svgz", "tga",
        "wbmp", "xcf",
    };
    return extensions;
}

std::string extensionOf(const std::string &path)
{
    const std::size_t slash = path.find_last_of("/\\");
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string::npos || dot + 1 == path.size()
        || (slash != std::string::npos && dot < slash)) {
        return {};
    }
    std::string extension = path.substr(dot + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return extension;
}

bool isExoticExtension(const std::string &extension)
{
    static const std::unordered_set<std::string> extensions(
        exoticExtensions().begin(), exoticExtensions().end());
    return extensions.find(extension) != extensions.end();
}

PkString importMimeType(const std::string &extension)
{
    // These formats have dedicated Krita import filters. The other exotic
    // QImage formats remain advertised to preserve the old accepted extension
    // set, but are a graceful GAP until a static high-level filter owns them.
    static const std::unordered_map<std::string, const char *> mimeTypes {
        {"hdr", "image/vnd.radiance"},
        {"psd", "image/vnd.adobe.photoshop"},
        {"svg", "image/svg+xml"},
        {"svgz", "image/svg+xml"},
        {"tga", "image/x-tga"},
        {"xcf", "image/x-xcf"},
    };
    const auto found = mimeTypes.find(extension);
    return found == mimeTypes.end() ? PkString() : PkString(found->second);
}

PkImage decodeWithImportFilter(const std::string &path)
{
    const PkString mimeType = importMimeType(extensionOf(path));
    if (mimeType.isEmpty()) {
        return {};
    }

    try {
        PkScopedPointer<KisDocument> document(
            KisDocumentRegistry::instance()->createTemporaryDocument());
        if (!document) {
            return {};
        }

        const std::string mimeUtf8 = mimeType.PkToUtf8();
        document->setMimeType(PkByteArray(mimeUtf8.data(),
                                          static_cast<int>(mimeUtf8.size())));
        document->setFileBatchMode(true);
        if (!document->importDocument(PkString(path.c_str()))) {
            return {};
        }

        KisImageSP image = document->image().toStrongRef();
        if (!image || image->bounds().isEmpty()) {
            return {};
        }
        return image->convertToQImage(image->bounds(), nullptr);
    } catch (...) {
        return {};
    }
}

void registerAdapter()
{
    PkImageFileDecoder::registerHandler({
        "kritaimpex-static-import",
        -1000,
        exoticExtensions(),
        [](const std::uint8_t *, std::size_t, const std::string &pathHint) {
            return isExoticExtension(extensionOf(pathHint));
        },
        [](const std::uint8_t *, std::size_t, const std::string &pathHint) {
            return decodeWithImportFilter(pathHint);
        },
    });
}

struct AdapterRegistration
{
    AdapterRegistration()
    {
        kisEnsurePkImageFileDecoderAdapterRegistered();
    }
};

AdapterRegistration s_adapterRegistration;

} // namespace

extern "C" void kisEnsurePkImageFileDecoderAdapterRegistered()
{
    static std::once_flag once;
    std::call_once(once, registerAdapter);
}
