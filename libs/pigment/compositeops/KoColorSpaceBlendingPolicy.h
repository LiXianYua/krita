/*
 *  SPDX-FileCopyrightText: 2023 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KOCOLORSPACEBLENDINGPOLICY_H
#define KOCOLORSPACEBLENDINGPOLICY_H

#include <kritapigment_export.h>

// PkStringList 仅用于下方返回值的**声明**（by-value 返回的声明只需不完整类型）。
// 不 include <KisQStringListFwd.h>：其 Qt6 分支把 PkStringList 声明成
// `using PkStringList = PkList<PkString>`（别名），而 pk/container/PkStringList.h
// 的真实定义是 `class PkStringList : public PkList<PkString>`（派生类）——两种形态
// 同 TU 共存即冲突。本头只需一个 `class PkStringList;` 前置声明，与真实类定义
// 兼容，也免去对 libs/global 该头的依赖。
class PkStringList;

/**
 * @brief default blending policy used in additive color spaces
 */
template<typename Traits>
struct KoAdditiveBlendingPolicy
{
    using channels_type = typename Traits::channels_type;
    inline static channels_type toAdditiveSpace(channels_type value) {
        return value;
    }

    inline static channels_type fromAdditiveSpace(channels_type value) {
        return value;
    }
};

/**
 * @brief a plending policy used for subtractive color spaces (e.g. CMYK)
 *
 * In CMYK we should first invert the colors to make them "additive",
 * and then blend.
 */
template<typename Traits>
struct KoSubtractiveBlendingPolicy
{
    using channels_type = typename Traits::channels_type;

    inline static channels_type toAdditiveSpace(channels_type value) {
        return Traits::math_trait::unitValue - value;
    }

    inline static channels_type fromAdditiveSpace(channels_type value) {
        return Traits::math_trait::unitValue - value;
    }
};

/**
 * @return false if the user selected the legacy behavior of the blendmodes in CMYK color spaces
 */
KRITAPIGMENT_EXPORT
bool useSubtractiveBlendingForCmykColorSpaces();

/**
 * @brief the list of blendmodes that perform channel-inversion in CMYK color space
 */
KRITAPIGMENT_EXPORT
PkStringList subtractiveBlendingModesInCmyk();

#endif // KOCOLORSPACEBLENDINGPOLICY_H
