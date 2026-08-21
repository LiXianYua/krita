/*
 *  SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2006 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <PkXmlCompat.h>

#include "KoRgbU8ColorSpace.h"

#include <limits.h>
#include <stdlib.h>


#include "KoChannelInfo.h"
#include "KoID.h"
#include "KoIntegerMaths.h"
#include "compositeops/KoCompositeOps.h"
#include "dithering/KisRgbDitherOpFactory.h"

#include "KoColorConversions.h"
#include <KoColorSpacePreserveLightnessUtils.h>

KoRgbU8ColorSpace::KoRgbU8ColorSpace() :

        KoSimpleColorSpace<KoBgrU8Traits>(colorSpaceId(),
                                          PkString("RGB (8-bit integer/channel, unmanaged)"),
                                          RGBAColorModelID,
                                          Integer8BitsColorDepthID)
{
    
    addChannel(new KoChannelInfo(PkString("Blue"),  0, 2, KoChannelInfo::COLOR, KoChannelInfo::UINT8, 1, PkColor(0, 0, 255)));
    addChannel(new KoChannelInfo(PkString("Green"), 1, 1, KoChannelInfo::COLOR, KoChannelInfo::UINT8, 1, PkColor(0, 255, 0)));
    addChannel(new KoChannelInfo(PkString("Red"),   2, 0, KoChannelInfo::COLOR, KoChannelInfo::UINT8, 1, PkColor(255, 0, 0)));
    addChannel(new KoChannelInfo(PkString("Alpha"), 3, 3, KoChannelInfo::ALPHA, KoChannelInfo::UINT8));

    // ADD, ALPHA_DARKEN, BURN, DIVIDE, DODGE, ERASE, MULTIPLY, OVER, OVERLAY, SCREEN, SUBTRACT
    addStandardCompositeOps<KoBgrU8Traits>(this);
    addStandardDitherOps<KoBgrU8Traits>(this);
}

KoRgbU8ColorSpace::~KoRgbU8ColorSpace()
{
}


PkString KoRgbU8ColorSpace::colorSpaceId()
{
    return PkString("RGBA");
}


KoColorSpace* KoRgbU8ColorSpace::clone() const
{
    return new KoRgbU8ColorSpace();
}


void KoRgbU8ColorSpace::fromQColor(const PkColor& c, quint8 *dst) const
{
    PkVector<float> channelValues;
    channelValues << c.blueF() << c.greenF() << c.redF() << c.alphaF();
    fromNormalisedChannelsValue(dst, channelValues);
}

void KoRgbU8ColorSpace::toQColor(const quint8 * src, PkColor *c) const
{
    PkVector<float> channelValues(4);
    normalisedChannelsValue(src, channelValues);
    c->setRgbF(channelValues[2], channelValues[1], channelValues[0], channelValues[3]);
}

void KoRgbU8ColorSpace::toHSY(const PkVector<double> &channelValues, qreal *hue, qreal *sat, qreal *luma) const
{
    
    RGBToHSY(channelValues[0],channelValues[1],channelValues[2], hue, sat, luma);
}

PkVector <double> KoRgbU8ColorSpace::fromHSY(qreal *hue, qreal *sat, qreal *luma) const
{
    PkVector <double> channelValues(4);
    HSYToRGB(*hue, *sat, *luma, &channelValues[0],&channelValues[1],&channelValues[2]);
    channelValues[3]=1.0;
    return channelValues;
}

void KoRgbU8ColorSpace::toYUV(const PkVector<double> &channelValues, qreal *y, qreal *u, qreal *v) const
{
    RGBToYUV(channelValues[0],channelValues[1],channelValues[2], y, u, v);
}

PkVector <double> KoRgbU8ColorSpace::fromYUV(qreal *y, qreal *u, qreal *v) const
{
    PkVector <double> channelValues(4);
    YUVToRGB(*y, *u, *v, &channelValues[0],&channelValues[1],&channelValues[2]);
    channelValues[3]=1.0;
    return channelValues;
}

void KoRgbU8ColorSpace::fillGrayBrushWithColorAndLightnessOverlay(quint8 *dst, const PkRgb *brush, quint8 *brushColor, qint32 nPixels) const
{
    fillGrayBrushWithColorPreserveLightnessRGB<KoBgrU8Traits>(dst, brush, brushColor, 1.0, nPixels);
}

void KoRgbU8ColorSpace::fillGrayBrushWithColorAndLightnessWithStrength(quint8* dst, const PkRgb* brush, quint8* brushColor, qreal strength, qint32 nPixels) const
{
    fillGrayBrushWithColorPreserveLightnessRGB<KoBgrU8Traits>(dst, brush, brushColor, strength, nPixels);
}

void KoRgbU8ColorSpace::modulateLightnessByGrayBrush(quint8 *dst, const PkRgb *brush, qreal strength, qint32 nPixels) const
{
   modulateLightnessByGrayBrushRGB<KoBgrU8Traits>(dst, brush, strength, nPixels);
}
