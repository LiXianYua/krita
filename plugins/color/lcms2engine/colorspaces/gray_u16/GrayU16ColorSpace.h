/*
 *  SPDX-FileCopyrightText: 2004-2006 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_COLORSPACE_GRAYSCALE_U16_H_
#define KIS_COLORSPACE_GRAYSCALE_U16_H_

#include "LcmsColorSpace.h"
#include <KoColorSpaceTraits.h>
#include "KoColorModelStandardIds.h"

class GrayAU16ColorSpace : public LcmsColorSpace<KoGrayU16Traits>
{
public:
    GrayAU16ColorSpace(const PkString &name, KoColorProfile *p);

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
        return Integer16BitsColorDepthID;
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
        return PkString("GRAYAU16");
    }
};

class GrayAU16ColorSpaceFactory : public LcmsColorSpaceFactory
{
public:
    GrayAU16ColorSpaceFactory()
        : LcmsColorSpaceFactory(TYPE_GRAYA_16, cmsSigGrayData)
    {
    }

    PkString id() const override
    {
        return GrayAU16ColorSpace::colorSpaceId();
    }

    PkString name() const override
    {
        return PkString("%1 (%2)").arg(GrayAColorModelID.name()).arg(Integer16BitsColorDepthID.name());
    }

    KoID colorModelId() const override
    {
        return GrayAColorModelID;
    }

    KoID colorDepthId() const override
    {
        return Integer16BitsColorDepthID;
    }

    int referenceDepth() const override
    {
        return 16;
    }

    bool userVisible() const override
    {
        return true;
    }

    KoColorSpace *createColorSpace(const KoColorProfile *p) const override
    {
        return new GrayAU16ColorSpace(name(), p->clone());
    }

    PkString defaultProfile() const override
    {
        return "Gray-D50-elle-V2-g10.icc";
    }
};

#endif // KIS_STRATEGY_COLORSPACE_GRAYSCALE_H_
