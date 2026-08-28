/*
 * SPDX-FileCopyrightText: 2026 S-09-c
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "pdf_import_policy.h"

#include <poppler-document.h>
#include <poppler-image.h>
#include <poppler-page.h>
#include <poppler-page-renderer.h>

#include <cstring>
#include <limits>
#include <memory>

bool renderPdfFirstPage(const char *data,
                        std::size_t size,
                        double resolutionPpi,
                        PdfRaster &raster)
{
    raster = {};
    if (!data || size == 0 || size > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        resolutionPpi <= 0.0) {
        return false;
    }

    std::unique_ptr<poppler::document> document(
        poppler::document::load_from_raw_data(data, static_cast<int>(size)));
    if (!document || document->is_locked() || document->pages() < 1 ||
        !poppler::page_renderer::can_render()) {
        return false;
    }

    std::unique_ptr<poppler::page> page(document->create_page(0));
    if (!page) {
        return false;
    }

    poppler::page_renderer renderer;
    renderer.set_render_hint(poppler::page_renderer::antialiasing);
    renderer.set_render_hint(poppler::page_renderer::text_antialiasing);
    renderer.set_image_format(poppler::image::format_argb32);
    const poppler::image image = renderer.render_page(page.get(), resolutionPpi, resolutionPpi);
    if (!image.is_valid() || image.width() <= 0 || image.height() <= 0 ||
        image.bytes_per_row() < image.width() * 4) {
        return false;
    }

    raster.width = image.width();
    raster.height = image.height();
    raster.stride = image.bytes_per_row();
    raster.argb.resize(static_cast<std::size_t>(raster.stride) * raster.height);
    std::memcpy(raster.argb.data(), image.const_data(), raster.argb.size());
    return true;
}
