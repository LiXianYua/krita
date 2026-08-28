/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger (cberger@cberger.net)
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "XyzF32ColorSpace.h"
#include <PkXmlElement.h>


#include "compositeops/KoCompositeOps.h"
#include "dithering/KisXyzDitherOpFactory.h"
#include <KoColorConversions.h>
#include <kis_dom_utils.h>

XyzF32ColorSpace::XyzF32ColorSpace(const PkString &name, KoColorProfile *p) :
    LcmsColorSpace<KoXyzF32Traits>(colorSpaceId(), name, TYPE_XYZA_FLT, cmsSigXYZData, p)
{
    const IccColorProfile *icc_p = dynamic_cast<const IccColorProfile *>(p);
    KIS_ASSERT(icc_p);
    PkVector<KoChannelInfo::DoubleRange> uiRanges(icc_p->getFloatUIMinMax());
    KIS_ASSERT(uiRanges.size() == 3);

    addChannel(new KoChannelInfo(PkString("X"),     KoXyzF32Traits::x_pos     * sizeof(float), KoXyzF32Traits::x_pos,     KoChannelInfo::COLOR, KoChannelInfo::FLOAT32, sizeof(float), PkColor(0, 255, 255), uiRanges[0]));
    addChannel(new KoChannelInfo(PkString("Y"),     KoXyzF32Traits::y_pos     * sizeof(float), KoXyzF32Traits::y_pos,     KoChannelInfo::COLOR, KoChannelInfo::FLOAT32, sizeof(float), PkColor(255, 0, 255), uiRanges[1]));
    addChannel(new KoChannelInfo(PkString("Z"),     KoXyzF32Traits::z_pos     * sizeof(float), KoXyzF32Traits::z_pos,     KoChannelInfo::COLOR, KoChannelInfo::FLOAT32, sizeof(float), PkColor(255, 255, 0), uiRanges[2]));
    addChannel(new KoChannelInfo(PkString("Alpha"), KoXyzF32Traits::alpha_pos * sizeof(float), KoXyzF32Traits::alpha_pos, KoChannelInfo::ALPHA, KoChannelInfo::FLOAT32, sizeof(float)));
    init();

    addStandardCompositeOps<KoXyzF32Traits>(this);
    addStandardDitherOps<KoXyzF32Traits>(this);
}

bool XyzF32ColorSpace::willDegrade(ColorSpaceIndependence independence) const
{
    if (independence == TO_RGBA16) {
        return true;
    } else {
        return false;
    }
}

KoColorSpace *XyzF32ColorSpace::clone() const
{
    return new XyzF32ColorSpace(name(), profile()->clone());
}

void XyzF32ColorSpace::colorToXML(const quint8 *pixel, PkXmlDocument &doc, PkXmlElement &colorElt) const
{
    const KoXyzF32Traits::Pixel *p = reinterpret_cast<const KoXyzF32Traits::Pixel *>(pixel);
    PkXmlElement labElt = doc.createElement("XYZ");
    labElt.setAttribute("x", KisDomUtils::toString(KoColorSpaceMaths< KoXyzF32Traits::channels_type, qreal>::scaleToA(p->x)));
    labElt.setAttribute("y", KisDomUtils::toString(KoColorSpaceMaths< KoXyzF32Traits::channels_type, qreal>::scaleToA(p->y)));
    labElt.setAttribute("z", KisDomUtils::toString(KoColorSpaceMaths< KoXyzF32Traits::channels_type, qreal>::scaleToA(p->z)));
    labElt.setAttribute("space", profile()->name());
    colorElt.appendChild(labElt);
}

void XyzF32ColorSpace::colorFromXML(quint8 *pixel, const PkXmlElement &elt) const
{
    KoXyzF32Traits::Pixel *p = reinterpret_cast<KoXyzF32Traits::Pixel *>(pixel);
    p->x = KoColorSpaceMaths< qreal, KoXyzF32Traits::channels_type >::scaleToA(KisDomUtils::toDouble(elt.attribute("x")));
    p->y = KoColorSpaceMaths< qreal, KoXyzF32Traits::channels_type >::scaleToA(KisDomUtils::toDouble(elt.attribute("y")));
    p->z = KoColorSpaceMaths< qreal, KoXyzF32Traits::channels_type >::scaleToA(KisDomUtils::toDouble(elt.attribute("z")));
    p->alpha = 1.0;
}

void XyzF32ColorSpace::toHSY(const PkVector<double> &channelValues, qreal *hue, qreal *sat, qreal *luma) const
{
    qreal xyx, xyy, xyY = 0.0;
    XYZToxyY(channelValues[0],channelValues[1],channelValues[2], &xyx, &xyy, &xyY);
    LabToLCH(xyY,xyx,xyY, hue, sat, luma);
}

PkVector <double> XyzF32ColorSpace::fromHSY(qreal *hue, qreal *sat, qreal *luma) const
{
    PkVector <double> channelValues(4);
    qreal xyx, xyy, xyY = 0.0;
    LCHToLab(*luma, *sat, *hue, &xyY,&xyx,&xyy);
    xyYToXYZ(xyx, xyy, xyY, &channelValues[0],&channelValues[1],&channelValues[2]);
    channelValues[3]=1.0;
    return channelValues;
}

void XyzF32ColorSpace::toYUV(const PkVector<double> &channelValues, qreal *y, qreal *u, qreal *v) const
{
    XYZToxyY(channelValues[0],channelValues[1],channelValues[2], u, v, y);
}

PkVector <double> XyzF32ColorSpace::fromYUV(qreal *y, qreal *u, qreal *v) const
{
    PkVector <double> channelValues(4);
    xyYToXYZ(*u, *v, *y, &channelValues[0],&channelValues[1],&channelValues[2]);
    channelValues[3]=1.0;
    return channelValues;
}
