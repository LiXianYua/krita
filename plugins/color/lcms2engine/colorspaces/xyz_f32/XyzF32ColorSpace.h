/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger (cberger@cberger.net)
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/

#ifndef KIS_XYZ_F32_COLORSPACE_H_
#define KIS_XYZ_F32_COLORSPACE_H_

#include <LcmsColorSpace.h>

#define TYPE_XYZA_FLT         (FLOAT_SH(1)|COLORSPACE_SH(PT_XYZ)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(4))

#include <KoColorModelStandardIds.h>

struct KoXyzF32Traits;

class XyzF32ColorSpace : public LcmsColorSpace<KoXyzF32Traits>
{
public:

    XyzF32ColorSpace(const PkString &name, KoColorProfile *p);

    bool willDegrade(ColorSpaceIndependence independence) const override;

    KoID colorModelId() const override
    {
        return XYZAColorModelID;
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
        return PkString("XYZAF32");
    }

    bool hasHighDynamicRange() const override
    {
        return true;
    }
};

class XyzF32ColorSpaceFactory : public LcmsColorSpaceFactory
{
public:

    XyzF32ColorSpaceFactory()
        : LcmsColorSpaceFactory(TYPE_XYZA_FLT, cmsSigXYZData)
    {
    }

    PkString id() const override
    {
        return XyzF32ColorSpace::colorSpaceId();
    }

    PkString name() const override
    {
        return PkString("%1 (%2)").arg(XYZAColorModelID.name()).arg(Float32BitsColorDepthID.name());
    }

    bool userVisible() const override
    {
        return true;
    }

    KoID colorModelId() const override
    {
        return XYZAColorModelID;
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
        return new XyzF32ColorSpace(name(), p->clone());
    }

    PkString defaultProfile() const override
    {
        return "XYZ identity built-in";
    }

    bool isHdr() const override
    {
        return true;
    }
};

#endif
