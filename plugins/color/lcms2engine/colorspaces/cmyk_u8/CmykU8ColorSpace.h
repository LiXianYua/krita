/*
 *  SPDX-FileCopyrightText: 2006 Cyrille Berger <cberger@cberger.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/

#ifndef KIS_STRATEGY_COLORSPACE_CMYK_U8_H_
#define KIS_STRATEGY_COLORSPACE_CMYK_U8_H_

#include <LcmsColorSpace.h>
#include <KoCmykColorSpaceTraits.h>
#include "KoColorModelStandardIds.h"

class CmykU8ColorSpace : public LcmsColorSpace<KoCmykU8Traits>
{
public:
    CmykU8ColorSpace(const PkString &name, KoColorProfile *p);

    bool willDegrade(ColorSpaceIndependence independence) const override;

    KoID colorModelId() const override
    {
        return CMYKAColorModelID;
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
        return PkString("CMYK");
    }

};

class CmykU8ColorSpaceFactory : public LcmsColorSpaceFactory
{
public:
    CmykU8ColorSpaceFactory()
        : LcmsColorSpaceFactory(TYPE_CMYK5_8, cmsSigCmykData)
    {

    }

    bool userVisible() const override
    {
        return true;
    }

    PkString id() const override
    {
        return CmykU8ColorSpace::colorSpaceId();
    }

    PkString name() const override
    {
        return PkString("%1 (%2)").arg(CMYKAColorModelID.name()).arg(Integer8BitsColorDepthID.name());
    }

    KoID colorModelId() const override
    {
        return CMYKAColorModelID;
    }

    KoID colorDepthId() const override
    {
        return Integer8BitsColorDepthID;
    }

    int referenceDepth() const override
    {
        return 8;
    }

    KoColorSpace *createColorSpace(const KoColorProfile *p) const override
    {
        return new CmykU8ColorSpace(name(), p->clone());
    }

    PkString defaultProfile() const override
    {
        return "Chemical proof";
    }
};

#endif
