/*
    SPDX-FileCopyrightText: 2026 S-03-a
    SPDX-License-Identifier: LGPL-2.1-or-later

    PkRgb —— 打包颜色值与其通道提取/打包辅助的零 Qt 垫片（S 线剥 Qt 用）。
    位布局对齐 Qt 5.15：0xAARRGGBB（对照 pk/image/tests/graft/qrgb_shim.h，
    逐字照抄真 Qt 5.15.7）。
    消费方：KoAlphaMaskApplicator*.h、KoColorSpaceTraits.h、KoColorSpace.cpp
    （fillGrayBrushWithColor 族）、KoColorSpacePreserveLightnessUtils.h、
    resources/KoAbstractGradient.cpp、resources/KoPattern.cpp、
    colorspaces/KoAlphaColorSpace.cpp。
 */

#ifndef PK_RGB_H
#define PK_RGB_H

#include <cstdint>

using PkRgb = std::uint32_t;   // 0xAARRGGBB，对齐 Qt 的打包色值

inline int pkRed(PkRgb rgb)   { return static_cast<int>((rgb >> 16) & 0xff); }
inline int pkGreen(PkRgb rgb) { return static_cast<int>((rgb >> 8) & 0xff); }
inline int pkBlue(PkRgb rgb)  { return static_cast<int>(rgb & 0xff); }
inline int pkAlpha(PkRgb rgb) { return static_cast<int>((rgb >> 24) & 0xff); }

inline PkRgb pkRgb(int r, int g, int b)
{
    return (0xffu << 24) | (static_cast<std::uint32_t>(r) << 16)
        | (static_cast<std::uint32_t>(g) << 8) | static_cast<std::uint32_t>(b);
}

inline PkRgb pkRgba(int r, int g, int b, int a)
{
    return (static_cast<std::uint32_t>(a) << 24) | (static_cast<std::uint32_t>(r) << 16)
        | (static_cast<std::uint32_t>(g) << 8) | static_cast<std::uint32_t>(b);
}

#endif // PK_RGB_H
