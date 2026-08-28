/*
 *  SPDX-FileCopyrightText: 2006 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "RgbF32ColorSpace.h"

#include <PkXmlElement.h>


#include "compositeops/KoCompositeOps.h"
#include "compositeops/RgbCompositeOps.h"
#include "dithering/KisRgbDitherOpFactory.h"
#include <KoColorConversions.h>
#include <KoColorSpaceMaths.h>
#include <KoColorSpacePreserveLightnessUtils.h>
#include <kis_dom_utils.h>

RgbF32ColorSpace::RgbF32ColorSpace(const PkString &name, KoColorProfile *p) :
    LcmsColorSpace<KoRgbF32Traits>(colorSpaceId(), name, TYPE_RGBA_FLT, cmsSigRgbData, p)
{
    const IccColorProfile *icc_p = dynamic_cast<const IccColorProfile *>(p);
    KIS_ASSERT(icc_p);
    PkVector<KoChannelInfo::DoubleRange> uiRanges(icc_p->getFloatUIMinMax());
    KIS_ASSERT(uiRanges.size() == 3);

    addChannel(new KoChannelInfo(PkString("Red"), 0 * sizeof(float), 0, KoChannelInfo::COLOR, KoChannelInfo::FLOAT32, 4, PkColor(255, 0, 0), uiRanges[0]));
    addChannel(new KoChannelInfo(PkString("Green"), 1 * sizeof(float), 1, KoChannelInfo::COLOR, KoChannelInfo::FLOAT32, 4, PkColor(0, 255, 0), uiRanges[1]));
    addChannel(new KoChannelInfo(PkString("Blue"), 2 * sizeof(float), 2, KoChannelInfo::COLOR, KoChannelInfo::FLOAT32, 4, PkColor(0, 0, 255), uiRanges[2]));
    addChannel(new KoChannelInfo(PkString("Alpha"), 3 * sizeof(float), 3, KoChannelInfo::ALPHA, KoChannelInfo::FLOAT32, 4));

    init();

    addStandardCompositeOps<KoRgbF32Traits>(this);
    addStandardDitherOps<KoRgbF32Traits>(this);

    addCompositeOp(new RgbCompositeOpIn<KoRgbF32Traits>(this));
    addCompositeOp(new RgbCompositeOpOut<KoRgbF32Traits>(this));
    addCompositeOp(new RgbCompositeOpBumpmap<KoRgbF32Traits>(this));
}

bool RgbF32ColorSpace::willDegrade(ColorSpaceIndependence independence) const
{
    if (independence == TO_RGBA16) {
        return true;
    } else {
        return false;
    }
}

KoColorSpace *RgbF32ColorSpace::clone() const
{
    return new RgbF32ColorSpace(name(), profile()->clone());
}

void RgbF32ColorSpace::colorToXML(const quint8 *pixel, PkXmlDocument &doc, PkXmlElement &colorElt) const
{
    const KoRgbF32Traits::Pixel *p = reinterpret_cast<const KoRgbF32Traits::Pixel *>(pixel);
    PkXmlElement labElt = doc.createElement("RGB");
    labElt.setAttribute("r", KisDomUtils::toString(KoColorSpaceMaths< KoRgbF32Traits::channels_type, qreal>::scaleToA(p->red)));
    labElt.setAttribute("g", KisDomUtils::toString(KoColorSpaceMaths< KoRgbF32Traits::channels_type, qreal>::scaleToA(p->green)));
    labElt.setAttribute("b", KisDomUtils::toString(KoColorSpaceMaths< KoRgbF32Traits::channels_type, qreal>::scaleToA(p->blue)));
    labElt.setAttribute("space", profile()->name());
    colorElt.appendChild(labElt);
}

void RgbF32ColorSpace::colorFromXML(quint8 *pixel, const PkXmlElement &elt) const
{
    KoRgbF32Traits::Pixel *p = reinterpret_cast<KoRgbF32Traits::Pixel *>(pixel);
    p->red = KoColorSpaceMaths< qreal, KoRgbF32Traits::channels_type >::scaleToA(KisDomUtils::toDouble(elt.attribute("r")));
    p->green = KoColorSpaceMaths< qreal, KoRgbF32Traits::channels_type >::scaleToA(KisDomUtils::toDouble(elt.attribute("g")));
    p->blue = KoColorSpaceMaths< qreal, KoRgbF32Traits::channels_type >::scaleToA(KisDomUtils::toDouble(elt.attribute("b")));
    p->alpha = 1.0;
}

void RgbF32ColorSpace::toHSY(const PkVector<double> &channelValues, qreal *hue, qreal *sat, qreal *luma) const
{
    RGBToHSY(channelValues[0],channelValues[1],channelValues[2], hue, sat, luma, lumaCoefficients()[0], lumaCoefficients()[1], lumaCoefficients()[2]);
}

PkVector <double> RgbF32ColorSpace::fromHSY(qreal *hue, qreal *sat, qreal *luma) const
{
    PkVector <double> channelValues(4);
    HSYToRGB(*hue, *sat, *luma, &channelValues[0],&channelValues[1],&channelValues[2], lumaCoefficients()[0], lumaCoefficients()[1], lumaCoefficients()[2]);
    channelValues[3]=1.0;
    return channelValues;
}

void RgbF32ColorSpace::toYUV(const PkVector<double> &channelValues, qreal *y, qreal *u, qreal *v) const
{

    
    RGBToYUV(channelValues[0],channelValues[1],channelValues[2], y, u, v, lumaCoefficients()[0], lumaCoefficients()[1], lumaCoefficients()[2]);
}

PkVector <double> RgbF32ColorSpace::fromYUV(qreal *y, qreal *u, qreal *v) const
{
    PkVector <double> channelValues(4);

    YUVToRGB(*y, *u, *v, &channelValues[0],&channelValues[1],&channelValues[2], lumaCoefficients()[0], lumaCoefficients()[1], lumaCoefficients()[2]);
    channelValues[3]=1.0;
    return channelValues;
}

void RgbF32ColorSpace::fillGrayBrushWithColorAndLightnessOverlay(quint8 *dst, const PkRgb *brush, quint8 *brushColor, qint32 nPixels) const
{
    fillGrayBrushWithColorPreserveLightnessRGB<KoRgbF32Traits>(dst, brush, brushColor, 1.0, nPixels);
}

void RgbF32ColorSpace::fillGrayBrushWithColorAndLightnessWithStrength(quint8* dst, const PkRgb* brush, quint8* brushColor, qreal strength, qint32 nPixels) const
{
    fillGrayBrushWithColorPreserveLightnessRGB<KoRgbF32Traits>(dst, brush, brushColor, strength, nPixels);
}

void RgbF32ColorSpace::modulateLightnessByGrayBrush(quint8 *dst, const PkRgb *brush, qreal strength, qint32 nPixels) const
{
    modulateLightnessByGrayBrushRGB<KoRgbF32Traits>(dst, brush, strength, nPixels);
}

