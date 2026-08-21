/*
 *  SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2006 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <PkXmlCompat.h>

#include "KoRgbU16ColorSpace.h"

#include <limits.h>
#include <stdlib.h>


#include "KoChannelInfo.h"
#include "KoID.h"
#include "KoIntegerMaths.h"

#include "KoColorConversions.h"
#include "dithering/KisRgbDitherOpFactory.h"
#include <KoColorSpacePreserveLightnessUtils.h>

KoRgbU16ColorSpace::KoRgbU16ColorSpace() :
        KoSimpleColorSpace<KoBgrU16Traits>(colorSpaceId(),
                                           PkString("RGB (16-bit integer/channel, unmanaged)"),
                                           RGBAColorModelID,
                                           Integer16BitsColorDepthID)
{
    addStandardDitherOps<KoBgrU16Traits>(this);
}

KoRgbU16ColorSpace::~KoRgbU16ColorSpace()
{
}


PkString KoRgbU16ColorSpace::colorSpaceId()
{
    return PkString("RGBA16");
}

KoColorSpace* KoRgbU16ColorSpace::clone() const
{
    return new KoRgbU16ColorSpace();
}

void KoRgbU16ColorSpace::fromQColor(const PkColor& c, quint8 *dst) const
{
    PkVector<float> channelValues;
    channelValues << c.blueF() << c.greenF() << c.redF() << c.alphaF();
    fromNormalisedChannelsValue(dst, channelValues);
}

void KoRgbU16ColorSpace::toQColor(const quint8 * src, PkColor *c) const
{
    PkVector<float> channelValues(4);
    normalisedChannelsValue(src, channelValues);
    c->setRgbF(channelValues[2], channelValues[1], channelValues[0], channelValues[3]);
}
void KoRgbU16ColorSpace::toHSY(const PkVector<double> &channelValues, qreal *hue, qreal *sat, qreal *luma) const
{
    
    RGBToHSY(channelValues[0],channelValues[1],channelValues[2], hue, sat, luma);
}

PkVector <double> KoRgbU16ColorSpace::fromHSY(qreal *hue, qreal *sat, qreal *luma) const
{
    PkVector <double> channelValues(4);
    HSYToRGB(*hue, *sat, *luma, &channelValues[0],&channelValues[1],&channelValues[2]);
    channelValues[3]=1.0;
    return channelValues;

}

void KoRgbU16ColorSpace::toYUV(const PkVector<double> &channelValues, qreal *y, qreal *u, qreal *v) const
{
    RGBToYUV(channelValues[0],channelValues[1],channelValues[2], y, u, v);
}

PkVector <double> KoRgbU16ColorSpace::fromYUV(qreal *y, qreal *u, qreal *v) const
{
    PkVector <double> channelValues(4);
    YUVToRGB(*y, *u, *v, &channelValues[0],&channelValues[1],&channelValues[2]);
    channelValues[3]=1.0;
    return channelValues;
}

void KoRgbU16ColorSpace::fillGrayBrushWithColorAndLightnessOverlay(quint8 *dst, const PkRgb *brush, quint8 *brushColor, qint32 nPixels) const
{
    fillGrayBrushWithColorPreserveLightnessRGB<KoBgrU16Traits>(dst, brush, brushColor, 1.0, nPixels);
}

void KoRgbU16ColorSpace::fillGrayBrushWithColorAndLightnessWithStrength(quint8* dst, const PkRgb* brush, quint8* brushColor, qreal strength, qint32 nPixels) const
{
    fillGrayBrushWithColorPreserveLightnessRGB<KoBgrU16Traits>(dst, brush, brushColor, strength, nPixels);
}

void KoRgbU16ColorSpace::modulateLightnessByGrayBrush(quint8 *dst, const PkRgb *brush, qreal strength, qint32 nPixels) const
{
    modulateLightnessByGrayBrushRGB<KoBgrU16Traits>(dst, brush, strength, nPixels);
}
