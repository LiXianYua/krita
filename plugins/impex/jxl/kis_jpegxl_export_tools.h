/*
 *  SPDX-FileCopyrightText: 2021 the JPEG XL Project Authors
 *  SPDX-License-Identifier: BSD-3-Clause
 *
 *  SPDX-FileCopyrightText: 2022 L. E. Segovia <amy@amyspark.me>
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  SPDX-FileCopyrightText: 2024 Rasyuqa A. H. <qampidh@gmail.com>
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_JPEGXL_EXPORT_TOOLS_H
#define KIS_JPEGXL_EXPORT_TOOLS_H

#include <PkAuxTypes.h>
#include "jxl_validation.h"

#include <limits>

#include <KisDocument.h>
#include <KoColorModelStandardIds.h>
#include <KoColorProfile.h>
#include <KoColorTransferFunctions.h>
#include <kis_iterator_ng.h>
#include <kis_types.h>

#include <jxl/encode_cxx.h>

namespace JXLExpTool
{
template<typename CSTrait>
inline PkByteArray
writeCMYKPixels(bool isTrichromatic, int chPos, const int width, const int height, KisHLineConstIteratorSP it)
{
    const int channels = isTrichromatic ? 3 : 1;
    const int chSize = static_cast<int>(CSTrait::pixelSize / 5);
    const int pxSize = chSize * channels;
    const int chOffset = chPos * chSize;

    PkByteArray res;
    std::size_t byteCount = 0;
    if (width <= 0 || height <= 0 || pxSize <= 0 ||
        !jxlCheckedImageBufferSize(static_cast<std::size_t>(width),
                                   static_cast<std::size_t>(height),
                                   static_cast<std::size_t>(channels),
                                   static_cast<std::size_t>(chSize),
                                   byteCount) ||
        byteCount > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return {};
    }
    res.resize(static_cast<int>(byteCount));

    quint8 *ptr = reinterpret_cast<quint8 *>(res.data());

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const quint8 *src = it->rawDataConst();

            if (isTrichromatic) {
                for (int i = 0; i < channels; i++) {
                    std::memcpy(ptr, src + (i * chSize), chSize);
                    ptr += chSize;
                }
            } else {
                std::memcpy(ptr, src + chOffset, chSize);
                ptr += chSize;
            }

            it->nextPixel();
        }

        it->nextRow();
    }
    return res;
}

template<typename... Args>
inline PkByteArray writeCMYKLayer(const KoID &id, Args &&...args)
{
    if (id == Integer8BitsColorDepthID) {
        return writeCMYKPixels<KoCmykU8Traits>(std::forward<Args>(args)...);
    } else if (id == Integer16BitsColorDepthID) {
        return writeCMYKPixels<KoCmykU16Traits>(std::forward<Args>(args)...);
#ifdef HAVE_OPENEXR
    } else if (id == Float16BitsColorDepthID) {
        return writeCMYKPixels<KoCmykF16Traits>(std::forward<Args>(args)...);
#endif
    } else if (id == Float32BitsColorDepthID) {
        return writeCMYKPixels<KoCmykF32Traits>(std::forward<Args>(args)...);
    } else {
        KIS_ASSERT_X(false, "JPEGXLExport::writeLayer", "unsupported bit depth!");
        return PkByteArray();
    }
}

} // namespace JXLExpTool

namespace HDR
{
template<ConversionPolicy policy>
ALWAYS_INLINE float applyCurveAsNeeded(float value)
{
    if (policy == ConversionPolicy::ApplyPQ) {
        return applySmpte2048Curve(value);
    } else if (policy == ConversionPolicy::ApplyHLG) {
        return applyHLGCurve(value);
    } else if (policy == ConversionPolicy::ApplySMPTE428) {
        return applySMPTE_ST_428Curve(value);
    }
    return value;
}

template<typename CSTrait,
         bool swap,
         bool convertToRec2020,
         bool isLinear,
         ConversionPolicy conversionPolicy,
         typename DestTrait,
         bool removeOOTF>
inline PkByteArray writeLayer(const int width,
                             const int height,
                             KisHLineConstIteratorSP it,
                             float hlgGamma,
                             float hlgNominalPeak,
                             const KoColorSpace *cs)
{
    const int channels = static_cast<int>(CSTrait::channels_nb);
    PkVector<float> pixelValues(channels);
    PkVector<qreal> pixelValuesLinear(channels);
    const KoColorProfile *profile = cs->profile();
    const PkVector<qreal> lCoef = cs->lumaCoefficients();
    double *src = pixelValuesLinear.data();
    float *dst = pixelValues.data();

    PkByteArray res;
    std::size_t byteCount = 0;
    if (width <= 0 || height <= 0 ||
        !jxlCheckedImageBufferSize(static_cast<std::size_t>(width),
                                   static_cast<std::size_t>(height),
                                   1,
                                   static_cast<std::size_t>(DestTrait::pixelSize),
                                   byteCount) ||
        byteCount > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return {};
    }
    res.resize(static_cast<int>(byteCount));

    quint8 *ptr = reinterpret_cast<quint8 *>(res.data());

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            CSTrait::normalisedChannelsValue(it->rawDataConst(), pixelValues);
            if (!convertToRec2020 && !isLinear) {
                for (int i = 0; i < channels; i++) {
                    src[i] = static_cast<double>(dst[i]);
                }
                profile->linearizeFloatValue(pixelValuesLinear);
                for (int i = 0; i < channels; i++) {
                    dst[i] = static_cast<float>(src[i]);
                }
            }

            if (conversionPolicy == ConversionPolicy::ApplyHLG && removeOOTF) {
                removeHLGOOTF(dst, lCoef.constData(), hlgGamma, hlgNominalPeak);
            }

            for (int ch = 0; ch < channels; ch++) {
                if (ch == CSTrait::alpha_pos) {
                    dst[ch] = applyCurveAsNeeded<ConversionPolicy::KeepTheSame>(
                        dst[ch]);
                } else {
                    dst[ch] = applyCurveAsNeeded<conversionPolicy>(dst[ch]);
                }
            }

            if (swap) {
                std::swap(dst[0], dst[2]);
            }

            DestTrait::fromNormalisedChannelsValue(ptr, pixelValues);

            ptr += DestTrait::pixelSize;

            it->nextPixel();
        }

        it->nextRow();
    }

    return res;
}

template<typename CSTrait, bool swap>
inline PkByteArray writeLayerNoConversion(const int width,
                                         const int height,
                                         KisHLineConstIteratorSP it,
                                         float hlgGamma,
                                         float hlgNominalPeak,
                                         const KoColorSpace *cs)
{
    Q_UNUSED(hlgGamma);
    Q_UNUSED(hlgNominalPeak);
    Q_UNUSED(cs);

    const int channels = static_cast<int>(CSTrait::channels_nb);
    PkVector<float> pixelValues(channels);
    PkVector<qreal> pixelValuesLinear(channels);

    PkByteArray res;
    std::size_t byteCount = 0;
    if (width <= 0 || height <= 0 ||
        !jxlCheckedImageBufferSize(static_cast<std::size_t>(width),
                                   static_cast<std::size_t>(height),
                                   1,
                                   static_cast<std::size_t>(CSTrait::pixelSize),
                                   byteCount) ||
        byteCount > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return {};
    }
    res.resize(static_cast<int>(byteCount));

    quint8 *ptr = reinterpret_cast<quint8 *>(res.data());

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            auto *dst = reinterpret_cast<typename CSTrait::channels_type *>(ptr);

            std::memcpy(dst, it->rawDataConst(), CSTrait::pixelSize);

            if (swap) {
                std::swap(dst[0], dst[2]);
            }

            ptr += CSTrait::pixelSize;

            it->nextPixel();
        }

        it->nextRow();
    }

    return res;
}

template<typename CSTrait,
         bool swap,
         bool convertToRec2020,
         bool isLinear,
         ConversionPolicy linearizePolicy,
         typename DestTrait,
         bool removeOOTF,
         typename... Args>
ALWAYS_INLINE auto writeLayerSimplify(Args &&...args)
{
    if (linearizePolicy != ConversionPolicy::KeepTheSame) {
        return writeLayer<CSTrait, swap, convertToRec2020, isLinear, linearizePolicy, DestTrait, removeOOTF>(
            std::forward<Args>(args)...);
    } else {
        return writeLayerNoConversion<CSTrait, swap>(std::forward<Args>(args)...);
    }
}

template<typename CSTrait,
         bool swap,
         bool convertToRec2020,
         bool isLinear,
         ConversionPolicy linearizePolicy,
         typename DestTrait,
         typename... Args>
ALWAYS_INLINE auto writeLayerWithPolicy(bool removeOOTF, Args &&...args)
{
    if (removeOOTF) {
        return writeLayerSimplify<CSTrait, swap, convertToRec2020, isLinear, linearizePolicy, DestTrait, true>(
            std::forward<Args>(args)...);
    } else {
        return writeLayerSimplify<CSTrait, swap, convertToRec2020, isLinear, linearizePolicy, DestTrait, false>(
            std::forward<Args>(args)...);
    }
}

template<typename CSTrait, bool swap, bool convertToRec2020, bool isLinear, typename... Args>
ALWAYS_INLINE auto writeLayerWithLinear(ConversionPolicy linearizePolicy, Args &&...args)
{
    if (linearizePolicy == ConversionPolicy::ApplyHLG) {
        return writeLayerWithPolicy<CSTrait,
                                    swap,
                                    convertToRec2020,
                                    isLinear,
                                    ConversionPolicy::ApplyHLG,
                                    KoBgrU16Traits>(std::forward<Args>(args)...);
    } else if (linearizePolicy == ConversionPolicy::ApplyPQ) {
        return writeLayerWithPolicy<CSTrait,
                                    swap,
                                    convertToRec2020,
                                    isLinear,
                                    ConversionPolicy::ApplyPQ,
                                    KoBgrU16Traits>(std::forward<Args>(args)...);
    } else if (linearizePolicy == ConversionPolicy::ApplySMPTE428) {
        return writeLayerWithPolicy<CSTrait,
                                    swap,
                                    convertToRec2020,
                                    isLinear,
                                    ConversionPolicy::ApplySMPTE428,
                                    KoBgrU16Traits>(std::forward<Args>(args)...);
    } else {
        return writeLayerWithPolicy<CSTrait, swap, convertToRec2020, isLinear, ConversionPolicy::KeepTheSame, CSTrait>(
            std::forward<Args>(args)...);
    }
}

template<typename CSTrait, bool swap, bool convertToRec2020, typename... Args>
ALWAYS_INLINE auto writeLayerWithRec2020(bool isLinear, Args &&...args)
{
    if (isLinear) {
        return writeLayerWithLinear<CSTrait, swap, convertToRec2020, true>(std::forward<Args>(args)...);
    } else {
        return writeLayerWithLinear<CSTrait, swap, convertToRec2020, false>(std::forward<Args>(args)...);
    }
}

template<typename CSTrait, bool swap, typename... Args>
ALWAYS_INLINE auto writeLayerWithSwap(bool convertToRec2020, Args &&...args)
{
    if (convertToRec2020) {
        return writeLayerWithRec2020<CSTrait, swap, true>(std::forward<Args>(args)...);
    } else {
        return writeLayerWithRec2020<CSTrait, swap, false>(std::forward<Args>(args)...);
    }
}

template<typename... Args>
inline auto writeLayer(const KoID &id, Args &&...args)
{
    if (id == Integer8BitsColorDepthID) {
        return writeLayerWithSwap<KoBgrU8Traits, true>(std::forward<Args>(args)...);
    } else if (id == Integer16BitsColorDepthID) {
        return writeLayerWithSwap<KoBgrU16Traits, true>(std::forward<Args>(args)...);
#ifdef HAVE_OPENEXR
    } else if (id == Float16BitsColorDepthID) {
        return writeLayerWithSwap<KoBgrF16Traits, false>(std::forward<Args>(args)...);
#endif
    } else if (id == Float32BitsColorDepthID) {
        return writeLayerWithSwap<KoBgrF32Traits, false>(std::forward<Args>(args)...);
    } else {
        KIS_ASSERT_X(false, "JPEGXLExport::writeLayer", "unsupported bit depth!");
        return PkByteArray();
    }
}
} // namespace HDR

#endif // KIS_JPEGXL_EXPORT_TOOLS_H
