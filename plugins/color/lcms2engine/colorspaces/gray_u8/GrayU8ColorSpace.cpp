/*
 *  SPDX-FileCopyrightText: 2006 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <GrayU8ColorSpace.h>

#include <PkXmlElement.h>


#include <KoIntegerMaths.h>
#include <KoColorSpaceRegistry.h>

#include "compositeops/KoCompositeOps.h"
#include "dithering/KisGrayDitherOpFactory.h"
#include <kis_dom_utils.h>

GrayAU8ColorSpace::GrayAU8ColorSpace(const PkString &name, KoColorProfile *p)
    : LcmsColorSpace<KoGrayU8Traits>(colorSpaceId(), name,  TYPE_GRAYA_8, cmsSigGrayData, p)
{
    addChannel(new KoChannelInfo(PkString("Gray"), 0, 0, KoChannelInfo::COLOR, KoChannelInfo::UINT8));
    addChannel(new KoChannelInfo(PkString("Alpha"), 1, 1, KoChannelInfo::ALPHA, KoChannelInfo::UINT8));

    init();

    addStandardCompositeOps<KoGrayU8Traits>(this);
    addStandardDitherOps<KoGrayU8Traits>(this);
}

KoColorSpace *GrayAU8ColorSpace::clone() const
{
    return new GrayAU8ColorSpace(name(), profile()->clone());
}

void GrayAU8ColorSpace::colorToXML(const quint8 *pixel, PkXmlDocument &doc, PkXmlElement &colorElt) const
{
    const KoGrayU8Traits::channels_type *p = reinterpret_cast<const KoGrayU8Traits::channels_type *>(pixel);
    PkXmlElement labElt = doc.createElement("Gray");
    labElt.setAttribute("g", KisDomUtils::toString(KoColorSpaceMaths< KoGrayU8Traits::channels_type, qreal>::scaleToA(p[0])));
    labElt.setAttribute("space", profile()->name());
    colorElt.appendChild(labElt);
}

void GrayAU8ColorSpace::colorFromXML(quint8 *pixel, const PkXmlElement &elt) const
{
    KoGrayU8Traits::channels_type *p = reinterpret_cast<KoGrayU8Traits::channels_type *>(pixel);
    p[0] = KoColorSpaceMaths< qreal, KoGrayU8Traits::channels_type >::scaleToA(KisDomUtils::toDouble(elt.attribute("g")));
    p[1] = KoColorSpaceMathsTraits<quint8>::max;
}

void GrayAU8ColorSpace::toHSY(const PkVector<double> &channelValues, qreal *, qreal *, qreal *luma) const
{
    *luma = channelValues[0];
}

PkVector <double> GrayAU8ColorSpace::fromHSY(qreal *, qreal *, qreal *luma) const
{
    PkVector <double> channelValues(2);
    channelValues.fill(*luma);
    channelValues[1]=1.0;
    return channelValues;
}

void GrayAU8ColorSpace::toYUV(const PkVector<double> &channelValues, qreal *y, qreal *, qreal *) const
{
    *y = channelValues[0];
}

PkVector <double> GrayAU8ColorSpace::fromYUV(qreal *y, qreal *, qreal *) const
{
    PkVector <double> channelValues(2);
    channelValues.fill(*y);
    channelValues[1]=1.0;
    return channelValues;
}
