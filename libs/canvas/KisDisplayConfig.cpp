/*
 *  SPDX-FileCopyrightText: 2024 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KisDisplayConfig.h"

#include <KoColorProfile.h>
#include <PkDebug.h>

KisDisplayConfig::KisDisplayConfig()
    : profile(nullptr)
    , intent(KoColorConversionTransformation::internalRenderingIntent())
    , conversionFlags(KoColorConversionTransformation::internalConversionFlags())
    , isHDR(false)
{
}

KisDisplayConfig::KisDisplayConfig(const KoColorProfile *_profile,
                                   KoColorConversionTransformation::Intent _intent,
                                   KoColorConversionTransformation::ConversionFlags _conversionFlags,
                                   bool _isHDR)
    : profile(_profile)
    , intent(_intent)
    , conversionFlags(_conversionFlags)
    , isHDR(_isHDR)
{
}

bool KisDisplayConfig::operator==(const KisDisplayConfig &rhs) const
{
    return profile == rhs.profile &&
            intent == rhs.intent &&
            conversionFlags == rhs.conversionFlags && 
            isHDR == rhs.isHDR;
}

PkDebug operator<<(PkDebug debug, const KisDisplayConfig &value) {
    debug.nospace() << "KisDisplayConfig(";

    debug.nospace() << "profile: " << value.profile;

    if (value.profile) {
        debug.nospace() << " (" << value.profile->name() << ")";
    }
    debug.nospace() << ", ";
    debug.nospace() << "intent: " << value.intent << ", ";
    debug.nospace() << "conversionFlags: " << value.conversionFlags << ", ";
    debug.nospace() << "isHDR: " << value.isHDR;

    debug.nospace() << ")";
    return debug;
}


bool KisMultiSurfaceDisplayConfig::operator==(const KisMultiSurfaceDisplayConfig &rhs) const
{
    return
        uiProfile == rhs.uiProfile &&
        canvasProfile == rhs.canvasProfile &&
        intent == rhs.intent &&
        conversionFlags == rhs.conversionFlags &&
        isCanvasHDR == rhs.isCanvasHDR;
}
