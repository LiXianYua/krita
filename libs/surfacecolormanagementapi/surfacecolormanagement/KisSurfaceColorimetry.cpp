/*
 *  SPDX-FileCopyrightText: 2025 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisSurfaceColorimetry.h"

#include <PkDebug.h>
#include <PkString.h>

#include <sstream>
#include <string>

namespace KisSurfaceColorimetry {

PkDebug operator<<(PkDebug debug, const NamedPrimaries &value) {
    debug.nospace() << "NamedPrimaries(";
    switch (value) {
    case NamedPrimaries::primaries_unknown:
        debug.nospace() << "primaries_unknown";
        break;
    case NamedPrimaries::primaries_srgb:
        debug.nospace() << "primaries_srgb";
        break;
    case NamedPrimaries::primaries_bt2020:
        debug.nospace() << "primaries_bt2020";
        break;
    case NamedPrimaries::primaries_dci_p3:
        debug.nospace() << "primaries_dci_p3";
        break;
    case NamedPrimaries::primaries_display_p3:
        debug.nospace() << "primaries_display_p3";
        break;
    case NamedPrimaries::primaries_adobe_rgb:
        debug.nospace() << "primaries_adobe_rgb";
        break;
    }
    debug.nospace() << ")";
    return debug.space();
}

PkDebug operator<<(PkDebug debug, const NamedTransferFunction &value) {
    debug.nospace() << "NamedTransferFunction(";
    switch (value) {
    case NamedTransferFunction::transfer_function_unknown:
        debug.nospace() << "transfer_function_unknown";
        break;
    case NamedTransferFunction::transfer_function_bt1886:
        debug.nospace() << "transfer_function_bt1886";
        break;
    case NamedTransferFunction::transfer_function_gamma22:
        debug.nospace() << "transfer_function_gamma22";
        break;
    case NamedTransferFunction::transfer_function_gamma28:
        debug.nospace() << "transfer_function_gamma28";
        break;
    case NamedTransferFunction::transfer_function_ext_linear:
        debug.nospace() << "transfer_function_ext_linear";
        break;
    case NamedTransferFunction::transfer_function_srgb:
        debug.nospace() << "transfer_function_srgb";
        break;
    case NamedTransferFunction::transfer_function_ext_srgb:
        debug.nospace() << "transfer_function_ext_srgb";
        break;
    case NamedTransferFunction::transfer_function_st2084_pq:
        debug.nospace() << "transfer_function_st2084_pq";
        break;
    case NamedTransferFunction::transfer_function_st428:
        debug.nospace() << "transfer_function_st428";
        break;
    }
    debug.nospace() << ")";
    return debug.space();
}

PkDebug operator<<(PkDebug debug, const Luminance &value) {
    debug.nospace() << "Luminance(minLuminance: " << value.minLuminance
                    << ", maxLuminance: " << value.maxLuminance
                    << ", referenceLuminance: " << value.referenceLuminance << ")";
    return debug.space();
}

PkDebug operator<<(PkDebug debug, const MasteringLuminance &value) {
    debug.nospace() << "MasteringLuminance(minLuminance: " << value.minLuminance
                    << ", maxLuminance: " << value.maxLuminance << ")";
    return debug.space();
}

PkDebug operator<<(PkDebug debug, const ColorSpace &value) {

    debug.nospace() << "ColorSpace(";
    std::visit([&] (auto &&v) {debug.nospace() << "primaries: " << v; }, value.primaries);
    std::visit([&] (auto &&v) {debug.nospace() << ", transferFunction: " << v; }, value.transferFunction);
    if (value.luminance) {
        debug.nospace() << ", luminance: " << *value.luminance;
    } else {
        debug.nospace() << ", luminance: " << "<none>";
    }
    debug.nospace() << ")";
    return debug.space();
}

PkDebug operator<<(PkDebug debug, const MasteringInfo &value) {
    debug.nospace() << "MasteringInfo(primaries: " << value.primaries;
    debug.nospace() << ", luminance: " << value.luminance;
    if (value.maxCll) {
        debug.nospace() << ", maxCll: " << *value.maxCll;
    }
    if (value.maxFall) {
        debug.nospace() << ", maxFall: " << *value.maxFall;
    }
    debug.nospace() << ")";
    return debug.space();
}

PkDebug operator<<(PkDebug debug, const SurfaceDescription &value) {
    debug.nospace() << "SurfaceDescription(colorSpace: " << value.colorSpace;
    if (value.masteringInfo) {
        debug.nospace() << ", masteringInfo: " << *value.masteringInfo;
    }
    debug.nospace() << ")";
    return debug.space();
}

PkDebug operator<<(PkDebug debug, const KisSurfaceColorimetry::RenderIntent &value) {
    debug.nospace() << "RenderIntent(";
    switch (value) {
    case KisSurfaceColorimetry::RenderIntent::render_intent_perceptual:
        debug.nospace() << "render_intent_perceptual";
        break;
    case KisSurfaceColorimetry::RenderIntent::render_intent_relative:
        debug.nospace() << "render_intent_relative";
        break;
    case KisSurfaceColorimetry::RenderIntent::render_intent_saturation:
        debug.nospace() << "render_intent_saturation";
        break;
    case KisSurfaceColorimetry::RenderIntent::render_intent_absolute:
        debug.nospace() << "render_intent_absolute";
        break;
    case KisSurfaceColorimetry::RenderIntent::render_intent_relative_bpc:
        debug.nospace() << "render_intent_relative_bpc";
        break;
    }
    debug.nospace() << ")";
    return debug.space();
}

PkString SurfaceDescription::makeTextReport() const
{
    std::ostringstream report;

    report << "  Color Space:" << '\n';

    if (std::holds_alternative<KisSurfaceColorimetry::NamedPrimaries>(this->colorSpace.primaries)) {
        report << "    Primaries: NamedPrimaries(";
        switch (std::get<KisSurfaceColorimetry::NamedPrimaries>(this->colorSpace.primaries)) {
        case NamedPrimaries::primaries_unknown:
            report << "primaries_unknown";
            break;
        case NamedPrimaries::primaries_srgb:
            report << "primaries_srgb";
            break;
        case NamedPrimaries::primaries_bt2020:
            report << "primaries_bt2020";
            break;
        case NamedPrimaries::primaries_dci_p3:
            report << "primaries_dci_p3";
            break;
        case NamedPrimaries::primaries_display_p3:
            report << "primaries_display_p3";
            break;
        case NamedPrimaries::primaries_adobe_rgb:
            report << "primaries_adobe_rgb";
            break;
        }
        report << ")" << '\n';
    } else {
        auto col = std::get<KisSurfaceColorimetry::Colorimetry>(this->colorSpace.primaries);
        report << "    Primaries: " << '\n';
        auto red = col.red().toxy();
        report << "        Red: xy(x: " << red.x << ", y: " << red.y << ")" << '\n';
        auto green = col.green().toxy();
        report << "        Green: xy(x: " << green.x << ", y: " << green.y << ")" << '\n';
        auto blue = col.blue().toxy();
        report << "        Blue: xy(x: " << blue.x << ", y: " << blue.y << ")" << '\n';
        auto white = col.white().toxy();
        report << "        White: xy(x: " << white.x << ", y: " << white.y << ")" << '\n';
    }

    if (std::holds_alternative<KisSurfaceColorimetry::NamedTransferFunction>(this->colorSpace.transferFunction)) {
        report << "    Transfer Function: NamedTransferFunction(";
        switch (std::get<KisSurfaceColorimetry::NamedTransferFunction>(this->colorSpace.transferFunction)) {
        case NamedTransferFunction::transfer_function_unknown:
            report << "transfer_function_unknown";
            break;
        case NamedTransferFunction::transfer_function_bt1886:
            report << "transfer_function_bt1886";
            break;
        case NamedTransferFunction::transfer_function_gamma22:
            report << "transfer_function_gamma22";
            break;
        case NamedTransferFunction::transfer_function_gamma28:
            report << "transfer_function_gamma28";
            break;
        case NamedTransferFunction::transfer_function_ext_linear:
            report << "transfer_function_ext_linear";
            break;
        case NamedTransferFunction::transfer_function_srgb:
            report << "transfer_function_srgb";
            break;
        case NamedTransferFunction::transfer_function_ext_srgb:
            report << "transfer_function_ext_srgb";
            break;
        case NamedTransferFunction::transfer_function_st2084_pq:
            report << "transfer_function_st2084_pq";
            break;
        case NamedTransferFunction::transfer_function_st428:
            report << "transfer_function_st428";
            break;
        }
        report << ")" << '\n';
    } else {
        const uint32_t rawValue = std::get<uint32_t>(this->colorSpace.transferFunction);
        report << "    Transfer Function (gamma): " << rawValue << "(" << double(rawValue) / 10000.0 << ")" << '\n';
    }

    if (this->colorSpace.luminance) {
        auto lum = *this->colorSpace.luminance;
        report << "    Luminance: Luminance(minLuminance: " << lum.minLuminance
               << ", maxLuminance: " << lum.maxLuminance
               << ", referenceLuminance: " << lum.referenceLuminance << ")" << '\n';
    } else {
        report << "    Luminance: " << "<none>" << '\n';
    }

    if (this->masteringInfo) {
        report << "  Mastering Info:" << '\n';
        auto col = this->masteringInfo->primaries;
        report << "    Primaries: " << '\n';
        auto red = col.red().toxy();
        report << "        Red: xy(x: " << red.x << ", y: " << red.y << ")" << '\n';
        auto green = col.green().toxy();
        report << "        Green: xy(x: " << green.x << ", y: " << green.y << ")" << '\n';
        auto blue = col.blue().toxy();
        report << "        Blue: xy(x: " << blue.x << ", y: " << blue.y << ")" << '\n';
        auto white = col.white().toxy();
        report << "        White: xy(x: " << white.x << ", y: " << white.y << ")" << '\n';
        report << "    Luminance: MasteringLuminance(minLuminance: " << this->masteringInfo->luminance.minLuminance
               << ", maxLuminance: " << this->masteringInfo->luminance.maxLuminance << ")" << '\n';
        report << "    Max CLL: " << (this->masteringInfo->maxCll ? std::to_string(*this->masteringInfo->maxCll) : "<none>") << '\n';
        report << "    Max FALL: " << (this->masteringInfo->maxFall ? std::to_string(*this->masteringInfo->maxFall) : "<none>") << '\n';
    } else {
        report << "  Mastering Info: <none>" << '\n';
    }

    return PkString(report.str().c_str());
}

} // namespace KisSurfaceColorimetry

/* KISSURFACECOLORIMETRY_H */