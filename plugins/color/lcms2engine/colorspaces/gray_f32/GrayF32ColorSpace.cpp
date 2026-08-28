/*
 *  SPDX-FileCopyrightText: 2006 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "GrayF32ColorSpace.h"

#include <PkXmlElement.h>


#include <KoIntegerMaths.h>
#include <KoColorSpaceRegistry.h>

#include "compositeops/KoCompositeOps.h"
#include "dithering/KisGrayDitherOpFactory.h"
#include <kis_dom_utils.h>

GrayF32ColorSpace::GrayF32ColorSpace(const PkString &name, KoColorProfile *p)
    : LcmsColorSpace<KoGrayF32Traits>(colorSpaceId(), name,  TYPE_GRAYA_FLT, cmsSigGrayData, p)
{
    const IccColorProfile *icc_p = dynamic_cast<const IccColorProfile *>(p);
    KIS_ASSERT(icc_p);
    PkVector<KoChannelInfo::DoubleRange> uiRanges(icc_p->getFloatUIMinMax());
    KIS_ASSERT(uiRanges.size() == 1);

    addChannel(new KoChannelInfo(PkString("Gray"), 0 * sizeof(float), 0, KoChannelInfo::COLOR, KoChannelInfo::FLOAT32, sizeof(float), PkColor(160, 160, 164), uiRanges[0]));
    addChannel(new KoChannelInfo(PkString("Alpha"), 1 * sizeof(float), 1, KoChannelInfo::ALPHA, KoChannelInfo::FLOAT32, sizeof(float)));

    init();

    addStandardCompositeOps<KoGrayF32Traits>(this);
    addStandardDitherOps<KoGrayF32Traits>(this);
}

KoColorSpace *GrayF32ColorSpace::clone() const
{
    return new GrayF32ColorSpace(name(), profile()->clone());
}

void GrayF32ColorSpace::colorToXML(const quint8 *pixel, PkXmlDocument &doc, PkXmlElement &colorElt) const
{
    const KoGrayF32Traits::channels_type *p = reinterpret_cast<const KoGrayF32Traits::channels_type *>(pixel);
    PkXmlElement labElt = doc.createElement("Gray");
    labElt.setAttribute("g", KisDomUtils::toString(KoColorSpaceMaths< KoGrayF32Traits::channels_type, qreal>::scaleToA(p[0])));
    labElt.setAttribute("space", profile()->name());
    colorElt.appendChild(labElt);
}

void GrayF32ColorSpace::colorFromXML(quint8 *pixel, const PkXmlElement &elt) const
{
    KoGrayF32Traits::channels_type *p = reinterpret_cast<KoGrayF32Traits::channels_type *>(pixel);
    p[0] = KoColorSpaceMaths< qreal, KoGrayF32Traits::channels_type >::scaleToA(KisDomUtils::toDouble(elt.attribute("g")));
    p[1] = 1.0;
}

void GrayF32ColorSpace::toHSY(const PkVector<double> &channelValues, qreal *, qreal *, qreal *luma) const
{
    *luma = channelValues[0];
}

PkVector <double> GrayF32ColorSpace::fromHSY(qreal *, qreal *, qreal *luma) const
{
    PkVector <double> channelValues(2);
    channelValues.fill(*luma);
    channelValues[1]=1.0;
    return channelValues;
}

void GrayF32ColorSpace::toYUV(const PkVector<double> &channelValues, qreal *y, qreal *, qreal *) const
{
    *y = channelValues[0];
}

PkVector <double> GrayF32ColorSpace::fromYUV(qreal *y, qreal *, qreal *) const
{
    PkVector <double> channelValues(2);
    channelValues.fill(*y);
    channelValues[1]=1.0;
    return channelValues;
}
