/*
 * SPDX-FileCopyrightText: 2026 S-09-c
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

enum class PdfImportBackend {
    PopplerCpp
};

struct PdfRaster {
    int width {0};
    int height {0};
    int stride {0};
    std::vector<std::uint8_t> argb;
};

constexpr PdfImportBackend pdfImportBackend()
{
    return PdfImportBackend::PopplerCpp;
}

bool renderPdfFirstPage(const char *data,
                        std::size_t size,
                        double resolutionPpi,
                        PdfRaster &raster);
