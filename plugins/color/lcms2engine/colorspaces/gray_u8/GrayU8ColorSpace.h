/*
 *  SPDX-FileCopyrightText: 2004-2006 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_GRAY_COLORSPACE_H_
#define KIS_GRAY_COLORSPACE_H_

#include <LcmsColorSpace.h>
#include <KoColorSpaceTraits.h>
#include "KoColorModelStandardIds.h"

class GrayAU8ColorSpace : public LcmsColorSpace<KoGrayU8Traits>
{
public:

    GrayAU8ColorSpace(const PkString &name, KoColorProfile *p);

    bool willDegrade(ColorSpaceIndependence) const override
    {
        return false;
    }

    KoID colorModelId() const override
    {
        return GrayAColorModelID;
    }

    KoID colorDepthId() const override
    {
        return Integer8BitsColorDepthID;
    }

    virtual KoColorSpace *clone() const;

    void colorToXML(const quint8 *pixel, PkXmlDocument &doc, PkXmlElement &colorElt) const override;

    void colorFromXML(quint8* pixel, const PkXmlElement& elt) const override;
    
    void toHSY(const PkVector<double> &channelValues, qreal *hue, qreal *sat, qreal *luma) const override;
    PkVector <double> fromHSY(qreal *hue, qreal *sat, qreal *luma) const override;
    void toYUV(const PkVector<double> &channelValues, qreal *y, qreal *u, qreal *v) const override;
    PkVector <double> fromYUV(qreal *y, qreal *u, qreal *v) const override;

    static PkString colorSpaceId()
    {
        return PkString("GRAYA");
    }

};

class GrayAU8ColorSpaceFactory : public LcmsColorSpaceFactory
{
public:
    GrayAU8ColorSpaceFactory()
        : LcmsColorSpaceFactory(TYPE_GRAYA_8, cmsSigGrayData)
    {
    }

    PkString id() const override
    {
        return GrayAU8ColorSpace::colorSpaceId();
    }

    PkString name() const override
    {
        return PkString("%1 (%2)").arg(GrayAColorModelID.name()).arg(Integer8BitsColorDepthID.name());
    }

    KoID colorModelId() const override
    {
        return GrayAColorModelID;
    }

    KoID colorDepthId() const override
    {
        return Integer8BitsColorDepthID;
    }

    int referenceDepth() const override
    {
        return 8;
    }

    bool userVisible() const override
    {
        return true;
    }

    KoColorSpace *createColorSpace(const KoColorProfile *p) const override
    {
        return new GrayAU8ColorSpace(name(), p->clone());
    }

    PkString defaultProfile() const override
    {
        return "Gray-D50-elle-V2-srgbtrc.icc";
    }
};

#endif // KIS_STRATEGY_COLORSPACE_GRAYSCALE_H_
