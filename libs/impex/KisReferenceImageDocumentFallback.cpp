/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KisReferenceImageDocumentFallback.h"

#include <filesystem>

#include <KisDocument.h>
#include <KisReferenceImage.h>
#include <KisResourceThumbnailCodec.h>
#include <KoStore.h>

#include "KisDocumentRegistry.h"

PkImage loadReferenceImageFileWithDocumentFallback(const PkString &filename)
{
    PkImage image;

    std::error_code ec;
    if (std::filesystem::exists(std::filesystem::u8path(filename.PkToUtf8()), ec) &&
        std::filesystem::is_regular_file(std::filesystem::u8path(filename.PkToUtf8()), ec)) {
        image = KisResourceThumbnailCodec::loadPng(filename);
    }

    if (image.isNull()) {
        KisDocumentRegistry *registry = KisDocumentRegistry::instance();
        KisDocument *document = registry->createTemporaryDocument();
        if (document->openPath(filename, KisDocument::DontAddToRecent)) {
            image = document->image()->convertToQImage(document->image()->bounds(), 0);
        }
        registry->removeDocument(document);
    }

    // R-15 gap (登记)：原代码在返回前执行过一次显式 sRGB 色彩空间转换
    // （bug 416515 的 JPEG→PNG 显式 sRGB 标记问题）——这里不再执行，因为
    // PkImage 无色彩空间元数据字段，且 KisPngCodec 走 libpng 裸 buffer 后
    // 不再读取该元数据。像素值本身已是 sRGB 语义，原转换只是元数据操作，
    // 非像素操作。
    return image;
}

bool loadReferenceImageWithDocumentFallback(KisReferenceImage *reference, KoStore *store)
{
    return reference->loadImage(store, loadReferenceImageFileWithDocumentFallback);
}
