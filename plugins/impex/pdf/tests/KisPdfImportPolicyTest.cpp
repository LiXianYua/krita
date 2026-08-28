/*
 * SPDX-FileCopyrightText: 2026 S-09-c
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "pdf_import_policy.h"

#include <iostream>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>

namespace {

std::string makeOnePagePdf()
{
    std::ostringstream pdf;
    std::streamoff offsets[5] {};
    pdf << "%PDF-1.4\n";
    offsets[1] = pdf.tellp();
    pdf << "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n";
    offsets[2] = pdf.tellp();
    pdf << "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n";
    offsets[3] = pdf.tellp();
    pdf << "3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 12 8] "
           "/Contents 4 0 R >>\nendobj\n";
    const std::string content = "1 0 0 rg 0 0 12 8 re f\n";
    offsets[4] = pdf.tellp();
    pdf << "4 0 obj\n<< /Length " << content.size() << " >>\nstream\n"
        << content << "endstream\nendobj\n";
    const std::streamoff xref = pdf.tellp();
    pdf << "xref\n0 5\n0000000000 65535 f \n";
    for (int i = 1; i != 5; ++i) {
        pdf.width(10);
        pdf.fill('0');
        pdf << offsets[i] << " 00000 n \n";
    }
    pdf << "trailer\n<< /Size 5 /Root 1 0 R >>\nstartxref\n"
        << xref << "\n%%EOF\n";
    return pdf.str();
}

} // namespace

int main()
{
    if (pdfImportBackend() != PdfImportBackend::PopplerCpp) {
        std::cerr << "configured native Poppler C++ raster backend was not selected\n";
        return 1;
    }

    PdfRaster raster;
    const std::string pdf = makeOnePagePdf();
    if (!renderPdfFirstPage(pdf.data(), pdf.size(), 72.0, raster)) {
        std::cerr << "native Poppler C++ backend did not render the fixture\n";
        return 2;
    }
    if (raster.width != 12 || raster.height != 8 || raster.stride < 48 || raster.argb.empty()) {
        std::cerr << "native raster dimensions/stride are not faithful\n";
        return 3;
    }
    const std::size_t center = static_cast<std::size_t>(raster.height / 2) * raster.stride +
                               static_cast<std::size_t>(raster.width / 2) * 4;
    std::uint32_t centerArgb = 0;
    std::memcpy(&centerArgb, raster.argb.data() + center, sizeof(centerArgb));
    const std::uint8_t alpha = static_cast<std::uint8_t>(centerArgb >> 24);
    const std::uint8_t red = static_cast<std::uint8_t>(centerArgb >> 16);
    const std::uint8_t green = static_cast<std::uint8_t>(centerArgb >> 8);
    const std::uint8_t blue = static_cast<std::uint8_t>(centerArgb);
    if (alpha != 255 || red < 250 || green > 5 || blue > 5) {
        std::cerr << "native raster did not preserve exact opaque-red ARGB32 pixel contract\n";
        return 4;
    }
    return 0;
}
