/*
 *  SPDX-FileCopyrightText: 2023 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KoClipMaskApplicatorBase.h"
#include <PkRgb.h>

void KoClipMaskApplicatorBase::fallbackLuminanceMask(quint8 *pixels, quint8 *maskPixels, const int nPixels) const{
    const quint32 colorChannelsMask = 0x00FFFFFF;
    const float redLum = 0.2125f;
    const float greenLum = 0.7154f;
    const float blueLum = 0.0721f;
    const float normCoeff = 1.0f / 255.0f;

    const PkRgb *mP = reinterpret_cast<const PkRgb*>(maskPixels);
    PkRgb *sP = reinterpret_cast<PkRgb*>(pixels);

    for (int i = 0; i < nPixels; i++) {
        const PkRgb mask = *mP;
        const PkRgb shape = *sP;

        const float maskValue = pkAlpha(mask) * (redLum * pkRed(mask) + greenLum * pkGreen(mask) + blueLum * pkBlue(mask)) * normCoeff;

        const quint8 alpha = OptiRound<xsimd::generic, quint8>::roundScalar(maskValue * float(pkAlpha(shape) * normCoeff));

        *sP = (alpha << 24) | (shape & colorChannelsMask);

        sP++;
        mP++;
    }
}
