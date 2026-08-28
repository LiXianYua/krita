/*
 *  SPDX-FileCopyrightText: 2006 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "GrayF16ColorSpace.h"

#include <PkXmlElement.h>


#include <KoIntegerMaths.h>
#include <KoColorSpaceRegistry.h>

#include "compositeops/KoCompositeOps.h"
#include "dithering/KisGrayDitherOpFactory.h"
#include <kis_dom_utils.h>

GrayF16ColorSpace::GrayF16ColorSpace(const PkString &name, KoColorProfile *p)
    : LcmsColorSpace<KoGrayF16Traits>(colorSpaceId(), name,  TYPE_GRAYA_HALF_FLT, cmsSigGrayData, p)
{
    const IccColorProfile *icc_p = dynamic_cast<const IccColorProfile *>(p);
    KIS_ASSERT(icc_p);
    (void)icc_p;
    addChannel(new KoChannelInfo(PkString("Gray"), 0 * sizeof(half), 0, KoChannelInfo::COLOR, KoChannelInfo::FLOAT16, 2, PkColor(160, 160, 164)));
    addChannel(new KoChannelInfo(PkString("Alpha"), 1 * sizeof(half), 1, KoChannelInfo::ALPHA, KoChannelInfo::FLOAT16, 2));

    init();

    addStandardCompositeOps<KoGrayF16Traits>(this);
    addStandardDitherOps<KoGrayF16Traits>(this);
}

KoColorSpace *GrayF16ColorSpace::clone() const
{
    return new GrayF16ColorSpace(name(), profile()->clone());
}

void GrayF16ColorSpace::colorToXML(const quint8 *pixel, PkXmlDocument &doc, PkXmlElement &colorElt) const
{
    const KoGrayF16Traits::channels_type *p = reinterpret_cast<const KoGrayF16Traits::channels_type *>(pixel);
    PkXmlElement labElt = doc.createElement("Gray");
    labElt.setAttribute("g", KisDomUtils::toString(KoColorSpaceMaths< KoGrayF16Traits::channels_type, qreal>::scaleToA(p[0])));
    labElt.setAttribute("space", profile()->name());
    colorElt.appendChild(labElt);
}

void GrayF16ColorSpace::colorFromXML(quint8 *pixel, const PkXmlElement &elt) const
{
    KoGrayF16Traits::channels_type *p = reinterpret_cast<KoGrayF16Traits::channels_type *>(pixel);
    p[0] = KoColorSpaceMaths< qreal, KoGrayF16Traits::channels_type >::scaleToA(KisDomUtils::toDouble(elt.attribute("g")));
    p[1] = 1.0;
}

void GrayF16ColorSpace::toHSY(const PkVector<double> &channelValues, qreal *, qreal *, qreal *luma) const
{
    *luma = channelValues[0];
}

PkVector <double> GrayF16ColorSpace::fromHSY(qreal *, qreal *, qreal *luma) const
{
    PkVector <double> channelValues(2);
    channelValues.fill(*luma);
    channelValues[1]=1.0;
    return channelValues;
}

void GrayF16ColorSpace::toYUV(const PkVector<double> &channelValues, qreal *y, qreal *, qreal *) const
{
    *y = channelValues[0];
}

PkVector <double> GrayF16ColorSpace::fromYUV(qreal *y, qreal *, qreal *) const
{
    PkVector <double> channelValues(2);
    channelValues.fill(*y);
    channelValues[1]=1.0;
    return channelValues;
}
