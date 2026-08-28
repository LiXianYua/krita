/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger (cberger@cberger.net)
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/

#ifndef KIS_YCBCR_U8_COLORSPACE_H_
#define KIS_YCBCR_U8_COLORSPACE_H_

#include <LcmsColorSpace.h>

#include <KoColorModelStandardIds.h>

#define TYPE_YCbCrA_8 (COLORSPACE_SH(PT_YCbCr)|CHANNELS_SH(3)|BYTES_SH(1)|EXTRA_SH(1))

struct KoYCbCrU8Traits;

class YCbCrU8ColorSpace : public LcmsColorSpace<KoYCbCrU8Traits>
{
public:

    YCbCrU8ColorSpace(const PkString &name, KoColorProfile *p);

    bool willDegrade(ColorSpaceIndependence independence) const override;

    static PkString colorSpaceId()
    {
        return PkString("YCBCRA8");
    }

    KoID colorModelId() const override
    {
        return YCbCrAColorModelID;
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

};

class YCbCrU8ColorSpaceFactory : public LcmsColorSpaceFactory
{
public:

    YCbCrU8ColorSpaceFactory() : LcmsColorSpaceFactory(TYPE_YCbCrA_8, cmsSigYCbCrData)
    {
    }

    PkString id() const override
    {
        return YCbCrU8ColorSpace::colorSpaceId();
    }

    PkString name() const override
    {
        return PkString("%1 (%2)").arg(YCbCrAColorModelID.name()).arg(Integer8BitsColorDepthID.name());
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
        return Integer8BitsColorDepthID;
    }

    int referenceDepth() const override
    {
        return 8;
    }

    KoColorSpace *createColorSpace(const KoColorProfile *p) const override
    {
        return new YCbCrU8ColorSpace(name(), p->clone());
    }

    PkString defaultProfile() const override
    {
        return "ITU-R BT.709-6 YCbCr ICC V4 profile";
    }
};

#endif
