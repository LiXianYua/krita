#ifndef HEIF_CHROMA_DISPATCH_H
#define HEIF_CHROMA_DISPATCH_H

#include <libheif/heif.h>

enum class HeifInterleavedLayout {
    SdrRgb,
    SdrRgba,
    HdrRgb,
    HdrRgba,
    Unsupported
};

inline HeifInterleavedLayout heifInterleavedLayout(heif_chroma chroma)
{
    switch (chroma) {
    case heif_chroma_interleaved_RGB:
        return HeifInterleavedLayout::SdrRgb;
    case heif_chroma_interleaved_RGBA:
        return HeifInterleavedLayout::SdrRgba;
    case heif_chroma_interleaved_RRGGBB_LE:
    case heif_chroma_interleaved_RRGGBB_BE:
        return HeifInterleavedLayout::HdrRgb;
    case heif_chroma_interleaved_RRGGBBAA_LE:
    case heif_chroma_interleaved_RRGGBBAA_BE:
        return HeifInterleavedLayout::HdrRgba;
    default:
        return HeifInterleavedLayout::Unsupported;
    }
}

#endif
