/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger (cberger@cberger.net)
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "YCbCrU8ColorSpace.h"
#include <PkXmlElement.h>


#include "compositeops/KoCompositeOps.h"
#include "dithering/KisYCbCrDitherOpFactory.h"
#include <KoColorConversions.h>

#include <kis_dom_utils.h>

YCbCrU8ColorSpace::YCbCrU8ColorSpace(const PkString &name, KoColorProfile *p)
    : LcmsColorSpace<KoYCbCrU8Traits>(colorSpaceId(), name, TYPE_YCbCrA_8, cmsSigYCbCrData, p)
{
    addChannel(new KoChannelInfo(PkString("Y"),     KoYCbCrU8Traits::Y_pos     * sizeof(quint8), KoYCbCrU8Traits::Y_pos,     KoChannelInfo::COLOR, KoChannelInfo::UINT8, sizeof(quint8), PkColor(0, 255, 255)));
    addChannel(new KoChannelInfo(PkString("Cb"),    KoYCbCrU8Traits::Cb_pos    * sizeof(quint8), KoYCbCrU8Traits::Cb_pos,    KoChannelInfo::COLOR, KoChannelInfo::UINT8, sizeof(quint8), PkColor(255, 0, 255)));
    addChannel(new KoChannelInfo(PkString("Cr"),    KoYCbCrU8Traits::Cr_pos    * sizeof(quint8), KoYCbCrU8Traits::Cr_pos,    KoChannelInfo::COLOR, KoChannelInfo::UINT8, sizeof(quint8), PkColor(255, 255, 0)));
    addChannel(new KoChannelInfo(PkString("Alpha"), KoYCbCrU8Traits::alpha_pos * sizeof(quint8), KoYCbCrU8Traits::alpha_pos, KoChannelInfo::ALPHA, KoChannelInfo::UINT8, sizeof(quint8)));

    init();

    addStandardCompositeOps<KoYCbCrU8Traits>(this);
    addStandardDitherOps<KoYCbCrU8Traits>(this);
}

bool YCbCrU8ColorSpace::willDegrade(ColorSpaceIndependence /*independence*/) const
{
    return false;
}

KoColorSpace *YCbCrU8ColorSpace::clone() const
{
    return new YCbCrU8ColorSpace(name(), profile()->clone());
}

void YCbCrU8ColorSpace::colorToXML(const quint8 *pixel, PkXmlDocument &doc, PkXmlElement &colorElt) const
{
    const KoYCbCrU8Traits::Pixel *p = reinterpret_cast<const KoYCbCrU8Traits::Pixel *>(pixel);
    PkXmlElement labElt = doc.createElement("YCbCr");
    labElt.setAttribute("Y",  KisDomUtils::toString(KoColorSpaceMaths< KoYCbCrU8Traits::channels_type, qreal>::scaleToA(p->Y)));
    labElt.setAttribute("Cb", KisDomUtils::toString(KoColorSpaceMaths< KoYCbCrU8Traits::channels_type, qreal>::scaleToA(p->Cb)));
    labElt.setAttribute("Cr", KisDomUtils::toString(KoColorSpaceMaths< KoYCbCrU8Traits::channels_type, qreal>::scaleToA(p->Cr)));
    labElt.setAttribute("space", profile()->name());
    colorElt.appendChild(labElt);
}

void YCbCrU8ColorSpace::colorFromXML(quint8 *pixel, const PkXmlElement &elt) const
{
    KoYCbCrU8Traits::Pixel *p = reinterpret_cast<KoYCbCrU8Traits::Pixel *>(pixel);
    p->Y = KoColorSpaceMaths< qreal, KoYCbCrU8Traits::channels_type >::scaleToA(KisDomUtils::toDouble(elt.attribute("Y")));
    p->Cb = KoColorSpaceMaths< qreal, KoYCbCrU8Traits::channels_type >::scaleToA(KisDomUtils::toDouble(elt.attribute("Cb")));
    p->Cr = KoColorSpaceMaths< qreal, KoYCbCrU8Traits::channels_type >::scaleToA(KisDomUtils::toDouble(elt.attribute("Cr")));
    p->alpha = KoColorSpaceMathsTraits<quint8>::max;
}

void YCbCrU8ColorSpace::toHSY(const PkVector<double> &channelValues, qreal *hue, qreal *sat, qreal *luma) const
{
    LabToLCH(channelValues[0],channelValues[1],channelValues[2], luma, sat, hue);
}

PkVector <double> YCbCrU8ColorSpace::fromHSY(qreal *hue, qreal *sat, qreal *luma) const
{
    PkVector <double> channelValues(4);
    LCHToLab(*luma, *sat, *hue, &channelValues[0],&channelValues[1],&channelValues[2]);
    channelValues[3]=1.0;
    return channelValues;
}
void YCbCrU8ColorSpace::toYUV(const PkVector<double> &channelValues, qreal *y, qreal *u, qreal *v) const
{
    *y =channelValues[0];
    *u=channelValues[1];
    *v=channelValues[2];
}

PkVector <double> YCbCrU8ColorSpace::fromYUV(qreal *y, qreal *u, qreal *v) const
{
    PkVector <double> channelValues(4);
    channelValues[0]=*y;
    channelValues[1]=*u;
    channelValues[2]=*v;
    channelValues[3]=1.0;
    return channelValues;
}
