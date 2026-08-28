/*
 *  SPDX-FileCopyrightText: 2006 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "GrayU16ColorSpace.h"

#include <PkXmlElement.h>


#include <KoIntegerMaths.h>
#include <KoColorSpaceRegistry.h>

#include "compositeops/KoCompositeOps.h"
#include "dithering/KisGrayDitherOpFactory.h"
#include <kis_dom_utils.h>

GrayAU16ColorSpace::GrayAU16ColorSpace(const PkString &name, KoColorProfile *p)
    : LcmsColorSpace<KoGrayU16Traits>(colorSpaceId(), name,  TYPE_GRAYA_16, cmsSigGrayData, p)
{
    addChannel(new KoChannelInfo(PkString("Gray"), 0 * sizeof(quint16), 0, KoChannelInfo::COLOR, KoChannelInfo::UINT16));
    addChannel(new KoChannelInfo(PkString("Alpha"), 1 * sizeof(quint16), 1, KoChannelInfo::ALPHA, KoChannelInfo::UINT16));

    init();

    addStandardCompositeOps<KoGrayU16Traits>(this);
    addStandardDitherOps<KoGrayU16Traits>(this);
}

KoColorSpace *GrayAU16ColorSpace::clone() const
{
    return new GrayAU16ColorSpace(name(), profile()->clone());
}

void GrayAU16ColorSpace::colorToXML(const quint8 *pixel, PkXmlDocument &doc, PkXmlElement &colorElt) const
{
    const KoGrayU16Traits::channels_type *p = reinterpret_cast<const KoGrayU16Traits::channels_type *>(pixel);
    PkXmlElement labElt = doc.createElement("Gray");
    labElt.setAttribute("g", KisDomUtils::toString(KoColorSpaceMaths< KoGrayU16Traits::channels_type, qreal>::scaleToA(p[0])));
    labElt.setAttribute("space", profile()->name());
    colorElt.appendChild(labElt);
}

void GrayAU16ColorSpace::colorFromXML(quint8 *pixel, const PkXmlElement &elt) const
{
    KoGrayU16Traits::channels_type *p = reinterpret_cast<KoGrayU16Traits::channels_type *>(pixel);
    p[0] = KoColorSpaceMaths< qreal, KoGrayU16Traits::channels_type >::scaleToA(KisDomUtils::toDouble(elt.attribute("g")));
    p[1] = KoColorSpaceMathsTraits<quint16>::max;
}

void GrayAU16ColorSpace::toHSY(const PkVector<double> &channelValues, qreal *, qreal *, qreal *luma) const
{
    *luma = channelValues[0];
}

PkVector <double> GrayAU16ColorSpace::fromHSY(qreal *, qreal *, qreal *luma) const
{
    PkVector <double> channelValues(2);
    channelValues.fill(*luma);
    channelValues[1]=1.0;
    return channelValues;
}

void GrayAU16ColorSpace::toYUV(const PkVector<double> &channelValues, qreal *y, qreal *, qreal *) const
{
    *y = channelValues[0];
}

PkVector <double> GrayAU16ColorSpace::fromYUV(qreal *y, qreal *, qreal *) const
{
    PkVector <double> channelValues(2);
    channelValues.fill(*y);
    channelValues[1]=1.0;
    return channelValues;
}
