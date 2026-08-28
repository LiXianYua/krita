/*
 *  SPDX-FileCopyrightText: 2006 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "CmykU16ColorSpace.h"

#include <PkXmlElement.h>

#include "compositeops/KoCompositeOps.h"
#include "dithering/KisCmykDitherOpFactory.h"
#include <KoColorConversions.h>
#include <kis_dom_utils.h>

CmykU16ColorSpace::CmykU16ColorSpace(const PkString &name, KoColorProfile *p)
    : LcmsColorSpace<KoCmykU16Traits>(colorSpaceId(), name, TYPE_CMYKA_16, cmsSigCmykData, p)
{
    addChannel(new KoChannelInfo(PkString("Cyan"), 0 * sizeof(quint16), 0, KoChannelInfo::COLOR, KoChannelInfo::UINT16, sizeof(quint16), PkColor(0, 255, 255)));
    addChannel(new KoChannelInfo(PkString("Magenta"), 1 * sizeof(quint16), 1, KoChannelInfo::COLOR, KoChannelInfo::UINT16, sizeof(quint16), PkColor(255, 0, 255)));
    addChannel(new KoChannelInfo(PkString("Yellow"), 2 * sizeof(quint16), 2, KoChannelInfo::COLOR, KoChannelInfo::UINT16, sizeof(quint16), PkColor(255, 255, 0)));
    addChannel(new KoChannelInfo(PkString("Black"), 3 * sizeof(quint16), 3, KoChannelInfo::COLOR, KoChannelInfo::UINT16, sizeof(quint16), PkColor(0, 0, 0)));
    addChannel(new KoChannelInfo(PkString("Alpha"), 4 * sizeof(quint16), 4, KoChannelInfo::ALPHA, KoChannelInfo::UINT16, sizeof(quint16)));

    init();

    addStandardCompositeOps<KoCmykU16Traits>(this);
    addStandardDitherOps<KoCmykU16Traits>(this);
}

bool CmykU16ColorSpace::willDegrade(ColorSpaceIndependence independence) const
{
    if (independence == TO_RGBA8) {
        return true;
    } else {
        return false;
    }
}

KoColorSpace *CmykU16ColorSpace::clone() const
{
    return new CmykU16ColorSpace(name(), profile()->clone());
}

void CmykU16ColorSpace::colorToXML(const quint8 *pixel, PkXmlDocument &doc, PkXmlElement &colorElt) const
{
    const KoCmykU16Traits::Pixel *p = reinterpret_cast<const KoCmykU16Traits::Pixel *>(pixel);
    PkXmlElement labElt = doc.createElement("CMYK");
    labElt.setAttribute("c", KisDomUtils::toString(KoColorSpaceMaths<KoCmykU16Traits::channels_type, qreal>::scaleToA(p->cyan)));
    labElt.setAttribute("m", KisDomUtils::toString(KoColorSpaceMaths<KoCmykU16Traits::channels_type, qreal>::scaleToA(p->magenta)));
    labElt.setAttribute("y", KisDomUtils::toString(KoColorSpaceMaths<KoCmykU16Traits::channels_type, qreal>::scaleToA(p->yellow)));
    labElt.setAttribute("k", KisDomUtils::toString(KoColorSpaceMaths<KoCmykU16Traits::channels_type, qreal>::scaleToA(p->black)));
    labElt.setAttribute("space", profile()->name());
    colorElt.appendChild(labElt);
}

void CmykU16ColorSpace::colorFromXML(quint8 *pixel, const PkXmlElement &elt) const
{
    KoCmykU16Traits::Pixel *p = reinterpret_cast<KoCmykU16Traits::Pixel *>(pixel);
    p->cyan = KoColorSpaceMaths< qreal, KoCmykU16Traits::channels_type >::scaleToA(KisDomUtils::toDouble(elt.attribute("c")));
    p->magenta = KoColorSpaceMaths< qreal, KoCmykU16Traits::channels_type >::scaleToA(KisDomUtils::toDouble(elt.attribute("m")));
    p->yellow = KoColorSpaceMaths< qreal, KoCmykU16Traits::channels_type >::scaleToA(KisDomUtils::toDouble(elt.attribute("y")));
    p->black = KoColorSpaceMaths< qreal, KoCmykU16Traits::channels_type >::scaleToA(KisDomUtils::toDouble(elt.attribute("k")));
    p->alpha = KoColorSpaceMathsTraits<quint16>::max;
}

void CmykU16ColorSpace::toHSY(const PkVector<double> &channelValues, qreal *hue, qreal *sat, qreal *luma) const
{
    qreal c0 = channelValues[0];
    qreal c1 = channelValues[1];
    qreal c2 = channelValues[2];
    qreal c3 = channelValues[3];
    //we use HSI here because we can't linearise CMYK, and HSY doesn't work right with...
    CMYKToCMY(&c0, &c1, &c2, &c3);
    c0 = 1.0 - c0;
    c1 = 1.0 - c1;
    c2 = 1.0 - c2;
    RGBToHSI(c0, c1, c2, hue, sat, luma);
}

PkVector <double> CmykU16ColorSpace::fromHSY(qreal *hue, qreal *sat, qreal *luma) const
{
    PkVector <double> channelValues(5);
    channelValues.fill(1.0);
    HSIToRGB(*hue, *sat, *luma, &channelValues[0],&channelValues[1],&channelValues[2]);
    channelValues[0] = qBound(0.0,1.0-channelValues[0],1.0);
    channelValues[1] = qBound(0.0,1.0-channelValues[1],1.0);
    channelValues[2] = qBound(0.0,1.0-channelValues[2],1.0);
    CMYToCMYK(&channelValues[0],&channelValues[1],&channelValues[2],&channelValues[3]);
    return channelValues;
}

void CmykU16ColorSpace::toYUV(const PkVector<double> &channelValues, qreal *y, qreal *u, qreal *v) const
{
    qreal c0 = channelValues[0];
    qreal c1 = channelValues[1];
    qreal c2 = channelValues[2];
    qreal c3 = channelValues[3];
    CMYKToCMY(&c0, &c1, &c2, &c3);
    c0 = 1.0 - c0;
    c1 = 1.0 - c1;
    c2 = 1.0 - c2;
    RGBToYUV(c0, c1, c2, y, u, v, 0.33, 0.33, 0.33);
}

PkVector <double> CmykU16ColorSpace::fromYUV(qreal *y, qreal *u, qreal *v) const
{
    PkVector <double> channelValues(5);
    channelValues.fill(1.0);
    YUVToRGB(*y, *u, *v, &channelValues[0],&channelValues[1],&channelValues[2], 0.33, 0.33, 0.33);
    channelValues[0] = qBound(0.0,1.0-channelValues[0],1.0);
    channelValues[1] = qBound(0.0,1.0-channelValues[1],1.0);
    channelValues[2] = qBound(0.0,1.0-channelValues[2],1.0);
    CMYToCMYK(&channelValues[0],&channelValues[1],&channelValues[2],&channelValues[3]);
    return channelValues;
}
