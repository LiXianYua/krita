/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger (cberger@cberger.net)
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "YCbCrU16ColorSpace.h"
#include <PkXmlElement.h>


#include "compositeops/KoCompositeOps.h"
#include "dithering/KisYCbCrDitherOpFactory.h"
#include <KoColorConversions.h>
#include <kis_dom_utils.h>

YCbCrU16ColorSpace::YCbCrU16ColorSpace(const PkString &name, KoColorProfile *p)
    : LcmsColorSpace<KoYCbCrU16Traits>(colorSpaceId(), name, TYPE_YCbCrA_16, cmsSigYCbCrData, p)
{
    addChannel(new KoChannelInfo(PkString("Y"),     KoYCbCrU16Traits::Y_pos     * sizeof(quint16), KoYCbCrU16Traits::Y_pos,     KoChannelInfo::COLOR, KoChannelInfo::UINT16, sizeof(quint16), PkColor(0, 255, 255)));
    addChannel(new KoChannelInfo(PkString("Cb"),    KoYCbCrU16Traits::Cb_pos    * sizeof(quint16), KoYCbCrU16Traits::Cb_pos,    KoChannelInfo::COLOR, KoChannelInfo::UINT16, sizeof(quint16), PkColor(255, 0, 255)));
    addChannel(new KoChannelInfo(PkString("Cr"),    KoYCbCrU16Traits::Cr_pos    * sizeof(quint16), KoYCbCrU16Traits::Cr_pos,    KoChannelInfo::COLOR, KoChannelInfo::UINT16, sizeof(quint16), PkColor(255, 255, 0)));
    addChannel(new KoChannelInfo(PkString("Alpha"), KoYCbCrU16Traits::alpha_pos * sizeof(quint16), KoYCbCrU16Traits::alpha_pos, KoChannelInfo::ALPHA, KoChannelInfo::UINT16, sizeof(quint16)));

    init();

    addStandardCompositeOps<KoYCbCrU16Traits>(this);
    addStandardDitherOps<KoYCbCrU16Traits>(this);
}

bool YCbCrU16ColorSpace::willDegrade(ColorSpaceIndependence independence) const
{
    if (independence == TO_RGBA8) {
        return true;
    } else {
        return false;
    }
}

KoColorSpace *YCbCrU16ColorSpace::clone() const
{
    return new YCbCrU16ColorSpace(name(), profile()->clone());
}

void YCbCrU16ColorSpace::colorToXML(const quint8 *pixel, PkXmlDocument &doc, PkXmlElement &colorElt) const
{
    const KoYCbCrU16Traits::Pixel *p = reinterpret_cast<const KoYCbCrU16Traits::Pixel *>(pixel);
    PkXmlElement labElt = doc.createElement("YCbCr");
    labElt.setAttribute("Y",  KisDomUtils::toString(KoColorSpaceMaths< KoYCbCrU16Traits::channels_type, qreal>::scaleToA(p->Y)));
    labElt.setAttribute("Cb", KisDomUtils::toString(KoColorSpaceMaths< KoYCbCrU16Traits::channels_type, qreal>::scaleToA(p->Cb)));
    labElt.setAttribute("Cr", KisDomUtils::toString(KoColorSpaceMaths< KoYCbCrU16Traits::channels_type, qreal>::scaleToA(p->Cr)));
    labElt.setAttribute("space", profile()->name());
    colorElt.appendChild(labElt);
}

void YCbCrU16ColorSpace::colorFromXML(quint8 *pixel, const PkXmlElement &elt) const
{
    KoYCbCrU16Traits::Pixel *p = reinterpret_cast<KoYCbCrU16Traits::Pixel *>(pixel);
    p->Y = KoColorSpaceMaths< qreal, KoYCbCrU16Traits::channels_type >::scaleToA(KisDomUtils::toDouble(elt.attribute("Y")));
    p->Cb = KoColorSpaceMaths< qreal, KoYCbCrU16Traits::channels_type >::scaleToA(KisDomUtils::toDouble(elt.attribute("Cb")));
    p->Cr = KoColorSpaceMaths< qreal, KoYCbCrU16Traits::channels_type >::scaleToA(KisDomUtils::toDouble(elt.attribute("Cr")));
    p->alpha = KoColorSpaceMathsTraits<quint16>::max;
}

void YCbCrU16ColorSpace::toHSY(const PkVector<double> &channelValues, qreal *hue, qreal *sat, qreal *luma) const
{
    LabToLCH(channelValues[0],channelValues[1],channelValues[2], luma, sat, hue);
}

PkVector <double> YCbCrU16ColorSpace::fromHSY(qreal *hue, qreal *sat, qreal *luma) const
{
    PkVector <double> channelValues(4);
    LCHToLab(*luma, *sat, *hue, &channelValues[0],&channelValues[1],&channelValues[2]);
    channelValues[3]=1.0;
    return channelValues;
}
void YCbCrU16ColorSpace::toYUV(const PkVector<double> &channelValues, qreal *y, qreal *u, qreal *v) const
{
    *y =channelValues[0];
    *u=channelValues[1];
    *v=channelValues[2];
}

PkVector <double> YCbCrU16ColorSpace::fromYUV(qreal *y, qreal *u, qreal *v) const
{
    PkVector <double> channelValues(4);
    channelValues[0]=*y;
    channelValues[1]=*u;
    channelValues[2]=*v;
    channelValues[3]=1.0;
    return channelValues;
}
