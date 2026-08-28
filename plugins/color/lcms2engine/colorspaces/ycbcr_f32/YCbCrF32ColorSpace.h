/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger (cberger@cberger.net)
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/

#ifndef KIS_YCBCR_F32_COLORSPACE_H_
#define KIS_YCBCR_F32_COLORSPACE_H_

#include <LcmsColorSpace.h>

#include <KoColorModelStandardIds.h>

#define TYPE_YCbCrA_FLT (FLOAT_SH(1)|COLORSPACE_SH(PT_YCbCr)|CHANNELS_SH(3)|EXTRA_SH(1)|BYTES_SH(4))

struct KoYCbCrF32Traits;

class YCbCrF32ColorSpace : public LcmsColorSpace<KoYCbCrF32Traits>
{
public:

    YCbCrF32ColorSpace(const PkString &name, KoColorProfile *p);

    bool willDegrade(ColorSpaceIndependence independence) const override;

    static PkString colorSpaceId()
    {
        return PkString("YCBCRF32");
    }

    KoID colorModelId() const override
    {
        return YCbCrAColorModelID;
    }

    KoID colorDepthId() const override
    {
        return Float32BitsColorDepthID;
    }

    bool hasHighDynamicRange() const override
    {
        return true;
    }

    virtual KoColorSpace *clone() const;

    void colorToXML(const quint8 *pixel, PkXmlDocument &doc, PkXmlElement &colorElt) const override;

    void colorFromXML(quint8* pixel, const PkXmlElement& elt) const override;
    void toHSY(const PkVector<double> &channelValues, qreal *hue, qreal *sat, qreal *luma) const override;
    PkVector <double> fromHSY(qreal *hue, qreal *sat, qreal *luma) const override;
    void toYUV(const PkVector<double> &channelValues, qreal *y, qreal *u, qreal *v) const override;
    PkVector <double> fromYUV(qreal *y, qreal *u, qreal *v) const override;

};

class YCbCrF32ColorSpaceFactory : public LcmsColorSpaceFactory
{
public:

    YCbCrF32ColorSpaceFactory()
        : LcmsColorSpaceFactory(TYPE_YCbCrA_FLT, cmsSigYCbCrData)
    {
    }

    PkString id() const override
    {
        return YCbCrF32ColorSpace::colorSpaceId();
    }

    PkString name() const override
    {
        return PkString("%1 (%2)").arg(YCbCrAColorModelID.name()).arg(Float32BitsColorDepthID.name());
    }

    bool userVisible() const override
    {
        return true;
    }

    KoID colorModelId() const override
    {
        return YCbCrAColorModelID;
    }

    KoID colorDepthId() const override
    {
        return Float32BitsColorDepthID;
    }

    int referenceDepth() const override
    {
        return 32;
    }

    KoColorSpace *createColorSpace(const KoColorProfile *p) const override
    {
        return new YCbCrF32ColorSpace(name(), p->clone());
    }

    PkString defaultProfile() const override
    {
        return "ITU-R BT.709-6 YCbCr ICC V4 profile";
    }

    bool isHdr() const override
    {
        return true;
    }
};

#endif
