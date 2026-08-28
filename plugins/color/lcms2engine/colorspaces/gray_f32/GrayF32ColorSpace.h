/*
 *  SPDX-FileCopyrightText: 2004-2006 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef COLORSPACE_GRAYSCALE_F32_H_
#define COLORSPACE_GRAYSCALE_F32_H_

#include <KoColorModelStandardIds.h>
#include "LcmsColorSpace.h"

#if !defined(TYPE_GRAYA_FLT)
#define TYPE_GRAYA_FLT         (FLOAT_SH(1)|COLORSPACE_SH(PT_GRAY)|EXTRA_SH(1)|CHANNELS_SH(1)|BYTES_SH(4))
#endif

struct KoGrayF32Traits;

class GrayF32ColorSpace : public LcmsColorSpace<KoGrayF32Traits>
{
public:
    GrayF32ColorSpace(const PkString &name, KoColorProfile *p);

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
        return Float32BitsColorDepthID;
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
        return PkString("GRAYAF32");
    }

    bool hasHighDynamicRange() const override
    {
        return true;
    }
};

class GrayF32ColorSpaceFactory : public LcmsColorSpaceFactory
{
public:
    GrayF32ColorSpaceFactory()
        : LcmsColorSpaceFactory(TYPE_GRAYA_FLT, cmsSigGrayData)
    {
    }

    PkString id() const override
    {
        return GrayF32ColorSpace::colorSpaceId();
    }

    PkString name() const override
    {
        return PkString("%1 (%2)").arg(GrayAColorModelID.name()).arg(Float32BitsColorDepthID.name());
    }

    KoID colorModelId() const override
    {
        return GrayAColorModelID;
    }

    KoID colorDepthId() const override
    {
        return Float32BitsColorDepthID;
    }

    int referenceDepth() const override
    {
        return 32;
    }

    bool userVisible() const override
    {
        return true;
    }

    KoColorSpace *createColorSpace(const KoColorProfile *p) const override
    {
        return new GrayF32ColorSpace(name(), p->clone());
    }

    PkString defaultProfile() const override
    {
        return "Gray-D50-elle-V2-g10.icc";
    }

    bool isHdr() const override
    {
        return true;
    }
};

#endif // KIS_STRATEGY_COLORSPACE_GRAYSCALE_H_
